/*
 * compute.c
 *
 *  Copyright (C) 2008,2009,2010,2011 Stefan Bolus, University of Kiel, Germany
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "Kure.h"
#include "dddmp.h"

#include "compute.h"
#include "Relation.h"
#include "RelationWindow.h"
#include "Semaphore.h"
#include "DebugWindow.h" /* debug_window_assert_failed_func */
#include "Relview.h" /* rv_ask_rel_name, rv_get_gtk_builder */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/times.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <setjmp.h>

/* Linux' POSIX sempahore implementation doesn't support process
 * level semaphores.  */
/*#include <semaphore.h>*/

#undef VERBOSE
//#define VERBOSE 1 -- not working anymore

#undef MESSAGE
#define MESSAGE _message
static void _message (const char *fmt, ...)
{
#if VERBOSE
  va_list ap;
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
#endif
}

typedef struct _Timer
{
  GTimer * glib_timer;
  struct tms tms1, tms2;
} Timer;

static void _timer_ctor (Timer * t) {  t->glib_timer = g_timer_new (); }
static void _timer_dtor (Timer * t) { g_timer_destroy (t->glib_timer); }
static void _timer_start (Timer * t) { g_timer_start (t->glib_timer);
times (& t->tms1); }
static void _timer_stop (Timer * t) { g_timer_stop (t->glib_timer);
times (& t->tms2);}
static double _timer_secs_elapsed (Timer * t)
{ return g_timer_elapsed (t->glib_timer, NULL); }
static double _timer_user_secs_elapsed (Timer * t)
{ return (t->tms2.tms_utime - t->tms1.tms_utime) / (double) sysconf(_SC_CLK_TCK); }
//static double _timer_sys_secs_elapsed (Timer * t)
//{ return (t->tms2.tms_stime - t->tms1.tms_stime) / (double) sysconf(_SC_CLK_TCK); }


static gchar * _format_timings (Timer * timer)
{
	double secs = _timer_secs_elapsed(timer);
	double user_secs = _timer_user_secs_elapsed(timer);
	gchar * fmt, *s, *ret = NULL;

	if (secs >= 1000.0)
		fmt = "%.0f";
	else if (secs >= 100.0)
		fmt = "%.1f";
	else if (secs >= 10.0)
		fmt = "%.2f";
	else if (secs >= 1.0)
		fmt = "%.3f";
	else if (secs >= 0.1)
		fmt = "%.4f";
	else if (secs >= 0.01)
		fmt = "%.5f";
	else
		fmt = "%f";

	s = g_strdup_printf (fmt, secs);

	if (user_secs != 0.0)
		ret = g_strdup_printf ("EVAL-TIME: %s sec. "
				"; CPU-TIME: %.2f sec.", s, user_secs);
	else ret = g_strdup_printf ("EVAL-TIME: %s sec.", s);

	g_free (s);
	return ret;
}



/*******************************************************************************
 *                            Parallel Computation                             *
 *                                                                             *
 *                              Mon, 19 Apr 2010                               *
 ******************************************************************************/

/*! Serialize a bignumber to a stream without seeking. It can be read by
 * _bignum_deserialize_from_stream ().
 *
 * \author stb
 * \param fp The stream
 * \param n The big number
 */
static
void _bignum_serialize_to_stream (FILE * fp, mpz_t n)
{
	size_t ret = mpz_out_raw (fp, n);
	if (0 == ret)
		fprintf (stderr, "mpz_inp_raw: Unable to write number to stream.");
}

/*! Deserialize a bignumber from a stream without seeking, previously writte
 * to the stream using _bignum_serialize_to_stream ().
 *
 * \author stb
 * \param fp The stream
 * \param n Pointer to a big number to store the result in.
 */
static
void _bignum_deserialize_from_stream (FILE *fp, mpz_t /*out*/ n)
{
	size_t ret = mpz_inp_raw (n, fp);
	if (0 == ret) {
		fprintf (stderr, "mpz_inp_raw: Unable to read number. Using 1 instead.\n");
		mpz_set_si (n, 1);
	}
	else {
#ifdef VERBOSE
		gmp_fprintf(stderr, "Read GMP number %Zd from stream.\n", n);
#endif
	}
}

typedef enum _RvStatusCode
{
  UNKNOWN = 0,
  SUCCESS,
  SYNTAX_ERROR,
  EVAL_ERROR,
  USER_CANCELATION
} RvStatusCode;


static int write_status_code (FILE * fp, RvStatusCode code)
{ return fprintf (fp, "%4d", (int)code); }

static int read_status_code (FILE * fp, RvStatusCode * /*out*/ pcode)
{ return fscanf (fp, "%4d", (int*)pcode); }

static int write_message (FILE * fp, const char * msg)
{
	unsigned int len = strlen (msg);
	if (len > 9999) len = 9999;
	return fprintf (fp, "%4u%s", len, msg);
}

/*!
 * Returns 1 on success. The newly created string must be freed using
 * \ref g_free. */
static gboolean read_message (FILE * fp, char ** pmsg)
{
	if (!pmsg) return FALSE;
	else {
		unsigned int len;
		int ret = fscanf (fp, "%4u", &len);
		if (1 != ret || 0 == len) return FALSE;
		else {
			*pmsg = g_new0 (gchar, len + 1/*\0*/);
			ret = fread (*pmsg, len, 1, fp);
			if (1 != ret) {
				g_warning ("read_message: Unable to read error message of len %u.\n", len);
				g_free (pmsg);
				return FALSE;
			}
			else return TRUE;
		}
	}
}

/*! Process for the computation of a given term.
 *
 * \author stb
 * \param pipeOut The pipe to the parent process. The process will write it's
 *                status code and the result to this stream.
 * \param term The term to compute.
 * \param semId A semaphore which get unlocked by the process, when it finished
 *              the computation, regardless of a possible error.
 */
void _compute_term_process (Relview * rv, lua_State * L, FILE * pipeOut,
		const gchar * term, int semId)
{
	KureContext * context = rv_get_context(rv);
	Timer timer;
	KureError * kerr = NULL;
	KureRel * impl;

	_timer_ctor(&timer);
	_timer_start(&timer);
	impl = kure_lang_exec(L, term, &kerr);
	_timer_stop(&timer);

	if (!impl) {
		_timer_dtor(&timer);
		Semaphore_post (semId);
		write_status_code (pipeOut, EVAL_ERROR);
		write_message (pipeOut, kerr->message);
		kure_error_destroy(kerr);
		return;
	}
	else {
		gchar * timestr;
		mpz_t rows, cols;

		timestr = _format_timings(&timer);
		printf ("%s\n", timestr);
		g_free (timestr);
		printf ("-------------------------------------------\n");
		fflush (stdout);

		write_status_code (pipeOut, SUCCESS);

		MESSAGE ( "worker> sem_post () in child process.\n");
		/* wake the parent up, so it reads the result from us. */
		Semaphore_post (semId);

		mpz_init (rows); mpz_init (cols);

		kure_rel_get_rows (impl, rows);
		kure_rel_get_cols (impl, cols);

		_bignum_serialize_to_stream (pipeOut, rows);
		_bignum_serialize_to_stream (pipeOut, cols);

		mpz_clear (rows); mpz_clear (cols);

		  MESSAGE ("worker> Dddmp_cuddBddStore(...) started\n");
		  Dddmp_cuddBddStore (kure_context_get_manager(context), NULL,
							  kure_rel_get_bdd(impl), NULL, NULL,
							  DDDMP_MODE_DEFAULT, (Dddmp_VarInfoType) NULL,
							  NULL, pipeOut);
		  MESSAGE ("worker> Dddmp_cuddBddStore(...) finished\n");

		  /* Write the current random numbers to the stream. Without that trick,
			 * the random number generator in the main process won't increase and
			 * every time a process is forked the same random numbers will be
			 * generated. */
			{
				int r = random();
				long int cudd_r = Cudd_Random();
				MESSAGE("worker> Writing random numbers: %d %ld\n", r, cudd_r);

				fprintf(pipeOut, "%d %ld\n", r, cudd_r);
			}
	}

	_timer_dtor (&timer);

	return;
}


struct worker_ctrl_info
{
  pid_t child;
  int semId;
  GMutex * mutex, * syncMutex;
  FILE * pipeIn;
  GtkWidget * dialog;
  GtkWidget * buttonCancelEval;
  GTimer * timer;
  const gchar * relName;
  RvStatusCode statusCode;
  gchar * errmsg;
  Relview * rv;
  KureRel * result_impl; /*!< Filled by the worker thread. Contains
						   * the result in case of success. */
};


/*! Invoked, when the user want to cancel the running evaluation.
 * The evaluation will abort immediately.
 *
 * \author stb
 * \param button The button handle.
 * \param user_data The pointer to the worker_ctrl_info structure.
 */
void
on_buttonCancelEval_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  struct worker_ctrl_info *wci = (struct worker_ctrl_info*) user_data;

  /* send SIGKILL to the computation process, on a cancelation request. */

  MESSAGE ("controller> Sending SIGKILL to child process (pid: %d)\n", wci->child);
  kill (wci->child, SIGKILL);
  wci->statusCode = USER_CANCELATION;
  MESSAGE ("dialog> mutex lock\n");
  g_mutex_lock (wci->mutex);
  Semaphore_post (wci->semId);
}


/*! Timeout callback, which updates the elapsed time and the progress bar
 * in the appearing dialog during the evaluation process.
 *
 * \author stb
 * \param data The pointer to the worker_ctrl_info structure.
 * \return Returns TRUE to indicate, that the callback may called again.
 */
static
gboolean _timeout_func (gpointer data)
{
  struct worker_ctrl_info *wci = (struct worker_ctrl_info*) data;

  GtkBuilder * builder = rv_get_gtk_builder (wci->rv);
  GtkWidget *pb = GTK_WIDGET(gtk_builder_get_object(builder, "progressEval"));
  GtkWidget *label = GTK_WIDGET(gtk_builder_get_object(builder, "labelTimeElapsedEval"));
  gdouble telapsed = g_timer_elapsed (wci->timer, NULL);
  gchar *s = NULL;

  /* show the elapsed time in the format "hh:mm:ss" */
  {
    unsigned secs = ((unsigned) telapsed ) % 60,
      mins = ((unsigned)(telapsed / 60.0) % 60),
      hours = ((unsigned)(telapsed / 3600.0));

    s = g_strdup_printf ("%.2u:%.2u:%.2u", hours, mins, secs);
  }

  gtk_progress_bar_pulse (GTK_PROGRESS_BAR (pb));

  gtk_label_set_text (GTK_LABEL (label), s);
  g_free (s);

  return TRUE;
}


/*! Thread function for the process controller. Waits for the process to
 * terminate and reads it's result, if no error occured.
 *
 * \author stb
 * \param data The pointer to the worker_ctrl_info structure.
 */
static
void _worker_controller_thread (void *data)
{
	struct worker_ctrl_info *wci = (struct worker_ctrl_info*) data;
	int ret;

	MESSAGE("controller> started! Waiting for the worker process, or user cancelation ...\n");

	Semaphore_wait(wci->semId);

	/* check whether the user canceled the evaluation, or the child process
	 * finished it. */
	ret = g_mutex_trylock (wci->mutex) ? 0 : 1;


	if (0 == ret) /* worker finished first; we got the lock! */
	{
		int status;
		RvStatusCode code;

		MESSAGE("controller> Settings the buttons sensitivity to FALSE ... ");
		gdk_threads_enter();
		{
			gtk_widget_set_sensitive (wci->buttonCancelEval, FALSE);
		}
		gdk_threads_leave();
		MESSAGE("done\n");

		MESSAGE("controller> the worker finished evaluation. Check for errors.\n");

		g_mutex_unlock (wci->mutex);

		read_status_code(wci->pipeIn, &code);
		MESSAGE("controller> Status from worker process: %s\n", (code
				== SUCCESS) ? "SUCCESS" : "ERROR");
		wci->statusCode = code;

		if (SUCCESS == code)
		{
			MESSAGE("controller> child finished\n");

			/* wait until the main thread entered it's main loop */
			MESSAGE("controller> waiting for the main loop ...\n");
			g_mutex_lock (wci->syncMutex);
			MESSAGE("controller> main loop started ...\n");
			g_mutex_unlock (wci->syncMutex);

			MESSAGE("controller> dialog closed?\n");

			gdk_threads_enter();
			MESSAGE("controller> dialog closed?\n");
			{
				DdNode * bdd = NULL;
				mpz_t rows, cols;
				KureContext * context = rv_get_context(wci->rv);

				mpz_init(rows);
				mpz_init(cols);

				_bignum_deserialize_from_stream(wci->pipeIn, rows);
				_bignum_deserialize_from_stream(wci->pipeIn, cols);

#if VERBOSE
				fprintf (stderr, "_worker_controller_thread: Cudd_ReadOne (%p) has ref-count of %d\n",
						Cudd_ReadOne(kure_context_get_manager(context)),
						Cudd_ReadOne(kure_context_get_manager(context))->ref);
#endif

				MESSAGE("controller> Dddmp_cuddBddLoad (...) started.\n");
				bdd = Dddmp_cuddBddLoad(kure_context_get_manager(context),
						DDDMP_VAR_MATCHIDS, NULL,
						NULL, NULL, DDDMP_MODE_DEFAULT, NULL, wci->pipeIn);
				MESSAGE("controller> Dddmp_cuddBddLoad (...) finished.\n");
				assert (bdd);
#if VERBOSE
				fprintf (stderr, "_worker_controller_thread: Cudd_Ref(%p)\n", bdd);
#endif
				Cudd_Ref(bdd);

#if VERBOSE
				gmp_fprintf (stderr,"_worker_controller_thread: Calling kure_rel_new_from_bdd(context, %p, %Zd, %Zd)\n",
						bdd, rows, cols);
				fprintf (stderr, "_worker_controller_thread: Refs of node %p before new from bdd: %d (align: %d)\n",
						bdd, Cudd_Regular(bdd)->ref, &((DdNode*)NULL)->ref);
#endif
				wci->result_impl = kure_rel_new_from_bdd(context, bdd, rows, cols);
#if VERBOSE
				fprintf (stderr, "_worker_controller_thread: Refs of node %p before deref: %d\n", bdd, Cudd_Regular(bdd)->ref);
#endif
				Cudd_Deref(bdd);
				if (Cudd_IsNonConstant(bdd))
					Cudd_Deref(bdd); // NOTE: Workaround! Don't know why this is necessary.
				mpz_clear(rows);
				mpz_clear(cols);

#if VERBOSE
				fprintf (stderr, "kure_rel_new_from_bdd: sizeof(DdNode): %d\n", sizeof(DdNode));
				fprintf (stderr, "_worker_controller_thread: Refs of node %p after deref: %d\n", bdd, Cudd_Regular(bdd)->ref);
				fprintf (stderr, "_worker_controller_thread: ZERO->ref=%d\n", Cudd_ReadOne(kure_context_get_manager(context))->ref);
#endif

				/* tha case that a relation with the choosen name already exists, is
				 * handled in the main thread. */

				/* read the random numbers from the pipe */
				{
					int r;
					long int cudd_r;
					fscanf(wci->pipeIn, "%d %ld\n", &r, &cudd_r);
					MESSAGE("contoller> Read random numbers: %d %ld\n", r,
							cudd_r);
					srandom(r);
					Cudd_Srandom(cudd_r);
				}

				gdk_flush();
			}
			gdk_threads_leave();

			/* prevent the worker from becoming a zombie */
			do
			{
				ret = waitpid(wci->child, &status, 0);
				if (ret < 0 && errno != EINTR)
				{
					perror("waitpid");
					assert (FALSE);
				}
			} while (ret != wci->child);


			gdk_threads_enter();
			{
				g_signal_emit_by_name(GTK_DIALOG (wci->dialog), "close");
				gdk_flush();
			}
			gdk_threads_leave();
		}
		else /* code != SUCCESS (error during evaluation) */
		{
			MESSAGE("controller> error during evaluation (%s)!\n",
					(SYNTAX_ERROR == code) ? "syntax error" : (EVAL_ERROR
							== code) ? "evaluation error" : "unknown error");

			/* In case of an error, read the error message from the pipe. */
			read_message(wci->pipeIn, &wci->errmsg);

			/* wait until the main thread entered it main loop */
			MESSAGE("controller> waiting for the main loop ...\n");
			g_mutex_lock (wci->syncMutex);
			MESSAGE("controller> main loop started ...\n");
			g_mutex_unlock (wci->syncMutex);

			gdk_threads_enter();
			{
				g_signal_emit_by_name(GTK_DIALOG (wci->dialog), "close");
				//gdk_flush();
			}
			gdk_threads_leave();

			/* prevent the worker from becoming a zombie */
			do
			{
				ret = waitpid(wci->child, &status, 0);
				if (ret < 0)
				{
					perror("waitpid");
					assert (FALSE);
				}
			} while (ret != wci->child);
		}
	}
	else /* user cancelation */
	{
		MESSAGE("controller> mutex (%p) don't got the lock!\n", wci->mutex);

		MESSAGE("controller> user cancelation\n");

		/* in this case the user has already quit the dialog's
		 * main loop. */

		MESSAGE("controller> mutex (%p) unlock!\n", wci->mutex);
		g_mutex_unlock (wci->mutex);
	}

	MESSAGE("controller> exited.\n");
}


/*! Callback which get called, when the dialog entered it's main loop in
 * the gtk_dialog_run () routine. Sets a mutex which shows the controller
 * thread, that the dialog may get closed.
 *
 * \author stb
 * \param data The pointer to the worker_ctrl_info structure.
 * \return Return FALSE, so the beloning source get kicked from the
 *         dialogs main loop.
 */
static gboolean _sync_threads (gpointer data)
{
  struct worker_ctrl_info *wci = (struct worker_ctrl_info*) data;

  g_mutex_unlock (wci->syncMutex);

  return FALSE;
}



/*! After the fork () the parent process calls this routine. It shows the
 * dialog to cancel the evaluation and waits for the dialog to get closed
 * by either the controller thread (ie. when the child process finished, or
 * an error occured), or the user.
 *
 * \author stb
 * \param pipeIn The read-only pipe to the child process.
 * \param child The child PID.
 * \param semId The semaphore, the controller thread will use to control
 *              the child process.
 * \param relName The rel. name, the evaluation result should stored in.
 *                A current relation, if exist, will get removed.
 */
void _parent_process(FILE * pipeIn, pid_t child, int semId,
		const gchar *relName, const gchar *term, RvComputeFlags flags)
{
	GThread *thread = NULL;
	struct worker_ctrl_info wci = { 0 };
	guint timeoutId;
	GSource *timeoutSource;
	Relview * rv = rv_get_instance();
	GtkBuilder * builder = rv_get_gtk_builder(rv);
	gulong handler_id;

	wci.child = child;
	wci.semId = semId;
	wci.pipeIn = pipeIn;
	wci.relName = relName;
	wci.statusCode = UNKNOWN;
	wci.errmsg = NULL;
	wci.result_impl = NULL;

	/* create a dialog to interact with the user and also create a thread
	 * to control the worker process. When the user requests abortion,
	 * the controller thread will close the dialog's main loop. */

	wci.rv = rv;
	wci.dialog = GTK_WIDGET(gtk_builder_get_object(builder, "dialogEvalProgress"));
	gtk_window_set_transient_for (wci.dialog, GTK_WINDOW(eval_window_get_widget(eval_window_get_instance())));
	gtk_window_set_modal (wci.dialog, TRUE);
	/* GTK_WIN_POS_CENTER (as set in Glade) doesn't seem to keep the window
	 * centered after it popped up several times. GTK_WIN_POS_CENTER_ALWAYS
	 * seem to work even if the GTK Ref. says it is some sort of cursed. */
	gtk_window_set_position (wci.dialog, GTK_WIN_POS_CENTER_ALWAYS);
	wci.buttonCancelEval = GTK_WIDGET(gtk_builder_get_object(builder, "buttonCancelEval"));
	gtk_widget_set_sensitive (wci.buttonCancelEval, TRUE);

	{
		GError *error;

		wci.mutex = g_mutex_new ();
		wci.syncMutex = g_mutex_new ();

		g_mutex_lock (wci.syncMutex);

		thread = g_thread_create ((GThreadFunc) _worker_controller_thread,
				(gpointer) &wci,
				TRUE /*joinable*/, &error);
		if (NULL == thread)
		{
			MESSAGE("controller> %s\n", error->message);
			assert (FALSE);
		}
	}

	wci.timer = g_timer_new();
	MESSAGE("main> create_dialogEvalProgress () ...\n");

	handler_id = g_signal_connect (G_OBJECT (wci.buttonCancelEval), "clicked",
			G_CALLBACK (on_buttonCancelEval_clicked),
			(gpointer) &wci);

	timeoutId = g_timeout_add(60 /*ms*/, _timeout_func, (gpointer) &wci);

	g_idle_add(_sync_threads, &wci);
	gtk_dialog_run(GTK_DIALOG (wci.dialog));

	MESSAGE("main> gtk_dialog_run has finished ...\n");

	/* We left the main loop. That means, An event has occurred. Start cleanup
	 * an see what's happend. First, hide the window which show the progress
	 * bar. */
	gtk_widget_hide(wci.dialog);

	/* Because we use GtkBuilder and reuse our evaluation progress widget,
	 * we have to remove the previous handler so it isn't used next time. */
	g_signal_handler_disconnect(G_OBJECT(wci.buttonCancelEval), handler_id);

	timeoutSource = g_main_context_find_source_by_id(NULL /* default context */, timeoutId);
	g_source_destroy(timeoutSource);
	g_timer_destroy(wci.timer);

	/* wait for the controller */
	MESSAGE("main> wait for the process controller thread ...\n");
	g_thread_join(thread);

	MESSAGE("main> destroy mutexes and semaphores\n");
	MESSAGE("main> mutex (%p) unlock and free\n", wci.mutex);
	g_mutex_free (wci.mutex);
	g_mutex_free (wci.syncMutex);

	if (SUCCESS == wci.statusCode)
	/* At this point, the worker thread has transfered the relation into our
	 * global manager. */

	/* If a relation with the given name already exists, ask the user
	 * what to do (cancel, overwrite, enter another name). */
	{
		gboolean inserted;
		DdManager * table = kure_context_get_manager(kure_rel_get_context(wci.result_impl));
		Rel * localRel = rel_new_from_impl(wci.relName, wci.result_impl);

#if VERBOSE
    	printf ("_parent_process: Ref. Count of BDD %p of the newly "
    			"created Rel \"%s\" (%p, KureRel: %p) is %d\n",
    			kure_rel_get_bdd(rel_get_impl(localRel)),
    			rel_get_name (localRel), localRel, rel_get_impl(localRel),
    			Cudd_Regular(kure_rel_get_bdd(rel_get_impl(localRel)))->ref);

    	fprintf (stderr, "_parent_process: ZERO->ref=%d\n", Cudd_ReadOne(table)->ref);
    	Cudd_ReadOne(table)->type;
    	Cudd_DeadAreCounted();
#endif

#ifdef VERBOSE
		// Debug code
		Cudd_CheckZeroRef (table);
		Cudd_CheckKeys (table);
		cuddGarbageCollect(table, 1);
#endif


    	if (flags & RV_COMPUTE_FLAGS_FORCE_OVERWRITE) {
    		/* delete the existing relation, with the desired name,
    		 * if there is one. (See above.) */
    		RelManager * manager = rv_get_rel_manager(rv);

    		rel_manager_delete_by_name(manager, wci.relName);
    		rel_manager_insert(manager, localRel);
    		inserted = TRUE;
    	}
    	else inserted = rv_user_rename_or_not(rv, localRel);

#if VERBOSE
    	printf ("_parent_process: Ref. Count of BDD %p of the newly "
    			"created Rel \"%s\" (%p, KureRel: %p) is %d\n",
    			kure_rel_get_bdd(rel_get_impl(localRel)),
    			rel_get_name (localRel), localRel, rel_get_impl(localRel),
    			Cudd_Regular(kure_rel_get_bdd(rel_get_impl(localRel)))->ref);
#endif

		if (inserted) {
			relation_window_set_relation(relation_window_get_instance(), localRel);
		}
		else /* user canceled */ {
			rel_destroy(localRel);
		}

	}
	else if (USER_CANCELATION == wci.statusCode)
	{
		/* no nothing */
		MESSAGE("main> The user has canceled the evaluation.\n");
	}
	else /* wci->statusCode != SUCCESS */
	{
		gchar * errmsg = wci.errmsg;

		if ( !wci.errmsg)
			errmsg = "Unknown error.";

		rv_user_error_with_descr ("Evaluation failed", errmsg,
				"Further details are listed below. The term was\n\t\"%s\".",
				term);

		if ( wci.errmsg) {
			g_free (wci.errmsg);
			wci.errmsg = NULL;
		}
	}

	return;
}


/* Use to handle SIGSEGV in compute_term_mp. See also _sigsegv_in_child_handler below. */
jmp_buf _child_env;
/*!
 * Recover in case of an SIGSEGV. This is important, because the parent process would
 * idle in this case without abortion by the user.
 */
void _sigsegv_in_child_handler (int signal)
{
	longjmp (_child_env, signal);
}


/*! Evaluates the given term and stores the result in a relation with the
 * given name. If the evaluation takes too long a window appears and offers
 * the user a progress bar and a cancel button to abort the evaluation.
 *
 * \author stb
 * \date 14.04.2008
 * \param term The term so evaluate.
 * \param relName The destination relation.
 */
static void compute_term_mp (const gchar *term, const gchar *relNameOrig,
                             RvComputeFlags flags)
{
	Relview * rv = rv_get_instance();
	lua_State * L;

	MESSAGE ("main> Creating Lua state.\n");
	L = rv_lang_new_state(rv);
	if (!L)	{
		MESSAGE ("main> Unable to create Lua state.\n");
	}
	else {
		gchar * relName = g_strstrip (g_strdup (relNameOrig));
		int pipefd[2];
		pid_t child;
		int semId;

		pipe(pipefd);
		semId = Semaphore_new_private(S_IWUSR | S_IRUSR, 0 /* initial value */);

		child = fork();
		if (-1 == child)
		{
			rv_user_error_with_descr("Internal error", g_strerror(errno),
					"Unable to fork the process (see below)."
						"Trying again might solve the problem. Otherwise save your "
						"workspace and restart the system.");
			return;
		}
		else if (0 == child) /* child */
		{
			int sig = -1;
			FILE *pipeOut = fdopen(pipefd[1], "w");
			close(pipefd[0]);

			signal (SIGSEGV, _sigsegv_in_child_handler);

			sig = setjmp (_child_env);
			if (0 == sig) /* first try */
				_compute_term_process(rv, L, pipeOut, term, semId);
			else{
				Semaphore_post (semId);
				write_status_code (pipeOut, EVAL_ERROR);

				if (SIGSEGV == sig)
					write_message (pipeOut, "Caught SIGSEGV. Please save your current "
							"state and restart the systems.");
				else
					write_message (pipeOut, "Unknown signal caught.");
			}

			fclose(pipeOut);
			MESSAGE ("worker> Exit.\n");
			exit(0);
		}
		else /* parent */
		{
			FILE *pipeIn = fdopen(pipefd[0], "r");
			close(pipefd[1]);

			_parent_process(pipeIn, child, semId, relName, term, flags);

			fclose(pipeIn);
		}

		Semaphore_destroy(semId);
		g_free(relName);

		/* Close earlier? */
		lua_close(L);
	}
}


/*******************************************************************************
 *                           Sequential Computation                            *
 *                                                                             *
 *                              Mon, 19 Apr 2010                               *
 ******************************************************************************/

static int _lua_assert_func (lua_State * L)
{
	const char * name = lua_tostring(L,1);
#if VERBOSE
	fprintf (stderr, __FILE__": _lua_assert_func: Assertion \"%s\" failed\n", name);
#endif
	return 0;
}

/*!
 * Evaluates the given term and stores the result in a relation with the
 * given name. The evaluation stops either if it's done, or an error occurs.
 *
 * \note The routine uses neither threads, nor processes.
 *
 * \author stb
 * \date 17.07.2008
 * \param term The term so evaluate.
 * \param relName The destination relation.
 */
static void compute_term_seq (const gchar *term, const gchar *relNameOrig,
                              RvComputeFlags flags)
{
	lua_State * L;
	gchar * relName = g_strdup(relNameOrig);
	Relview * rv = rv_get_instance();

	g_strstrip (relName);

	if (strlen(relName) == 0) {
		g_warning ("compute_term_seq: Empty relation names are allowed here!\n");
		g_free (relName);
		return;
	}

	L = rv_lang_new_state(rv);
	if (!L)	{
		rv_user_error("We've got problems!",
				"Unable to create a Lua state. Sorry ...");
	}
	else {
		Timer timer;
		KureError * kerr = NULL;
		KureRel * impl;

		/* Set the callback for the assertions is necessary. */
		if (flags & RV_COMPUTE_FLAGS_CHECK_ASSERTIONS) {
			kure_lang_set_assert_func (L, debug_window_assert_failed_func);
		}

		_timer_ctor(&timer);
		_timer_start(&timer);
		impl = kure_lang_exec(L, term, &kerr);
		_timer_stop(&timer);
		if (!impl) {
			debug_window_error_func(L);
			rv_user_error_with_descr("Unable to evaluate term",
					kerr->message, "See below for further details. "
					"Your term was:\n\t%s.", term);
			kure_error_destroy(kerr);
		}
		else {
			gchar * timestr;

			debug_window_finished_func(L);

			timestr = _format_timings(&timer);
			printf ("%s\n", timestr);
			g_free (timestr);
		    printf ("-------------------------------------------\n");

		    /* Insert the new relation into the global manager and handle the
		     * case in which a relation with the given name already exists.
		     * See \ref rv_user_rename_or_not. */
		    {
		    	gboolean inserted;
		    	Rel * rel = rel_new_from_impl(relName, impl);

#if VERBOSE
    	printf ("compute_term_seq: Ref. Count of BDD %p of the newly "
    			"created Rel \"%s\" (%p, KureRel: %p) is %d\n",
    			kure_rel_get_bdd(rel_get_impl(rel)),
    			rel_get_name (rel), rel, rel_get_impl(rel),
    			Cudd_Regular(kure_rel_get_bdd(rel_get_impl(rel)))->ref);
#endif

		    	if (flags & RV_COMPUTE_FLAGS_FORCE_OVERWRITE) {
		    		/* delete the existing relation, with the desired name,
		    		 * if there is one. (See above.) */
		    		RelManager * manager = rv_get_rel_manager(rv);
		    		rel_manager_delete_by_name(manager, relName);
		    		rel_manager_insert(manager, rel);
		    		inserted = TRUE;
		    	}
		    	else inserted = rv_user_rename_or_not(rv, rel);

				if (inserted)
					relation_window_set_relation(relation_window_get_instance(), rel);
				else /* user cancelation */
					rel_destroy(rel);
		    }
		}

		_timer_dtor(&timer);
		lua_close(L);
	}

	g_free (relName);
	return;
}


/* --------------------------------- Computation Interface (compute_term) --- */

/*!
 * Evaluates the given term and stores the result in a relation with the
 * given name. Depending on `checkAssertions` the evaluation can be aborted
 * by the user or not.
 *
 * \author stb
 * \date 14.04.2008
 * \param term The term so evaluate.
 * \param relName The destination relation.
 * \param checkAssertions If TRUE, evaluation can't be aborted. If an assertion
 *        failed during evaluation, the debugger window will be opened.
 */
void compute_term (const gchar *term, const gchar *relNameOrig,
                   RvComputeFlags flags)
{

	/* Assertions can't be checked in multi-processing mode, so we use the
	 * sequential variant. */
	if (flags & RV_COMPUTE_FLAGS_CHECK_ASSERTIONS)
	{
		compute_term_seq(term, relNameOrig, flags);
	}
	else {
		if (flags & RV_COMPUTE_FLAGS_SEQUENTIAL)
		{
			compute_term_seq(term, relNameOrig, flags);
		}
		else {
			compute_term_mp (term, relNameOrig, flags);
		}
	}
}

