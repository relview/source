#ifndef __MSG_BOXES_H__
#define __MSG_BOXES_H__

#define NOTICE_YES           1
#define NOTICE_NO            2
#define NOTICE_OVERWRITE_ALL 3
#define NOTICE_CANCEL        4

#define NOTICE_INVALID -10

int overwrite_and_wait (const gchar *message);
int error_and_wait (const gchar *message);
int error_notifier (const gchar *message);

extern char *error_msg [];

enum error_num {
  overwrite_rel,
  overwrite_graph,
  rel_not_exists,
  rel_del,
  rel_del_all,
  fun_del_all,
  prog_del_all,
  dom_del_all,
  label_del_all,
  select_out_of_range,
  rel_term_err,
  rel_not_quad,
  rel_not_included,
  rel_async_graph,
  overwrite_fun,
  overwrite_prog,
  overwrite_dom,
  overwrite_label,
  really_quit,
  wrong_input,
  name_not_allowed,
  not_allowed,
  domop_not_allowed,
  basic_op_err,
  syntax_error ,
  fun_not_found,
  dom_not_found,
  fun_eval_err,
  token_mismatch,
  create_rel,
  create_fun,
  rel_dim,
  overwrite_file,
  save_error,
  save_error2,
  load_error,
  double_def,
  name_mismatch,
  out_of_sync,
  next_komp,
  no_vector,
  graph_edit_not_allowed,
  to_big_label_number,
  already_labeled,
  no_such_col_label,
  no_such_line_label,
  graph_algo_error
};

#endif
