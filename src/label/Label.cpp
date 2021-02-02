#include "label.h"
#include <deque>
#include <algorithm>
#include <map>
#include <cstring>
#include <cstdio>

using namespace std;

/*abstract*/ class _Label
{
private:
	gchar * _name;
	deque<LabelObserver*> _obs;
	gboolean _persistent;
	
public:
	_Label (const gchar * name) : _name(g_strdup(name)), _persistent(FALSE) {}
	virtual ~_Label () {
		deque<LabelObserver*>::iterator iter = _obs.begin();

		for ( ; iter != _obs.end() ; ++iter) {
			LabelObserver * o = *iter;
			o->onDelete (o->object, this);
		}

		g_free (_name);
	}
	
	const gchar * getName () const { return _name; }
	void setPersistent(gboolean yesno) { _persistent = yesno; }
	gboolean isPersistent () { return _persistent; }
	void registerObserver (LabelObserver * l) {
		if ( find(_obs.begin(), _obs.end(), l) == _obs.end())
			_obs.push_back (l);
	}
	void unregisterObserver (LabelObserver * l) {
		deque<LabelObserver*>::iterator iter = find(_obs.begin(), _obs.end(), l);
		if (iter != _obs.end())
			_obs.erase (iter);
	}

	virtual LabelIter * iterator () const = 0;
	virtual const gchar * getNth (int n, GString * s) const = 0;
	virtual size_t getMaxLen (int first, int last/*incl*/) const = 0;
};

/*abstract*/ class _LabelIter
{
public:
	virtual gboolean isValid () const = 0;
	virtual void next () = 0;
	virtual const gchar * name (GString * s) const = 0;
	virtual int number () const = 0;
	virtual void seek (int n) = 0;
};

class _LabelListIter;

class _LabelList : public Label
{
private:
	typedef map<int,gchar*> map_type;
	map_type _elems;

	friend class _LabelListIter;

public:
	_LabelList (const gchar * name) : Label(name) { ; }
	~_LabelList () {
		map_type::iterator iter = _elems.begin();
		for ( ; iter != _elems.begin() ; ++iter )
			g_free (iter->second);
	}

	gboolean add (int number, const gchar * name) {
		pair<map_type::iterator, bool> res = _elems.insert (map_type::value_type (number, NULL));
		if (res.second)
			res.first->second = g_strdup (name);
		return res.second;
	}

	/* Implementation of "Label"'s interface */
	LabelIter * iterator () const;
	const gchar * getNth (int n, GString * s) const {
		map_type::const_iterator iter = _elems.find(n);
		if (iter != _elems.end()) {
			g_string_assign (s, iter->second);
			return s->str;
		}
		else {
			g_string_assign(s, "");
			return NULL;
		}
	}
	size_t getMaxLen (int first, int last/*incl*/) const {
		size_t max_len = 0;
		map_type::const_iterator iter = _elems.begin ();
		while (iter != _elems.end() && iter->first < first) { ++iter; }

		for ( ; iter != _elems.end() && iter->first <= last  ; ++iter)
			max_len = max (max_len, strlen(iter->second));

		return max_len;
	}
};

class _LabelListIter : public LabelIter
{
private:
	const LabelList * _label;
	LabelList::map_type::const_iterator _iter;

	_LabelListIter (const LabelList * label)
		: _label(label), _iter(label->_elems.begin()) { ; }

	friend class _LabelList;
private:

	/* Implementation of "LabelIter"'s interface */
	gboolean isValid () const { return _iter != _label->_elems.end(); }
	void next () { ++_iter; }
	const gchar * name (GString * s) const { g_string_assign(s,_iter->second); return s->str; }
	int number () const { return _iter->first; }
	void seek (int n) {
		_iter = _label->_elems.begin();
		if (n > 0);
			while (_iter != _label->_elems.end() && _iter->first < n) ++_iter;
	}
};

LabelIter * LabelList::iterator () const { return new _LabelListIter (this); }


class _LabelFuncIter;

class _LabelFunc : public Label
{
private:
	LabelRawFunc 	_func;
	LabelMaxLenFunc _maxlen_func; // can be NULL
	gpointer 		_user_data;
	GFreeFunc 		_user_dtor;

	friend class LabelFuncIter;

public:
	/*!
	 * \param maxlen_func Can be NULL.
	 */
	_LabelFunc (const gchar * name, LabelRawFunc func, LabelMaxLenFunc maxlen_func,
			gpointer user_data, GFreeFunc user_dtor)
		: Label(name), _func(func), _maxlen_func(maxlen_func),
		  _user_data(user_data), _user_dtor(user_dtor) { ; }
	~_LabelFunc () {
		if (_user_dtor)
			_user_dtor(_user_data);
	}

	/* Implementation of "Label"'s interface */
	LabelIter * iterator () const;
	const gchar * getNth (int n, GString * s) const { return _func (n, s, _user_data); }
	size_t getMaxLen (int first, int last/*incl*/) const {
		if (_maxlen_func) {
			return _maxlen_func (first,last,_user_data);
		}
		else {
			size_t max_len = 0;
			GString * s = g_string_new ("");
			for (int i = first ; i <= last ; ++i)
				max_len = max(max_len, strlen(getNth(i, s)));
			g_string_free(s, TRUE);
			return max_len;
		}
	}
};

class _LabelFuncIter : public LabelIter
{
private:
	const LabelFunc * _label;
	int _n;

	_LabelFuncIter (const LabelFunc * label) : _label(label), _n(1) { ; }

	friend class _LabelFunc;
private:

	/* Implementation of "LabelIter"'s interface */
	gboolean isValid () const { return true; }
	void next () { ++_n; }
	const gchar * name (GString * s) const { return _label->getNth(_n,s); }
	int number () const { return _n; }
	void seek (int n) { _n = max(1,n); }
};

LabelIter * LabelFunc::iterator () const { return new _LabelFuncIter (this); }

/*!
 * C interface implementation
 */

void 			label_destroy (Label * self) { delete self; }
void			label_set_persistent (Label * self, gboolean yesno) { self->setPersistent(yesno); }
gboolean		label_is_persistent (Label * self) { return self->isPersistent(); }
const gchar * 	label_get_nth (Label * self, int n, GString * s) { return self->getNth(n,s); }
int				label_get_max_len (Label * self, int first, int last /*incl.*/) {
	return self->getMaxLen(first,last);
}
const gchar * 	label_get_name (Label * self) { return self->getName(); }
LabelIter *		label_iterator (Label * self) { return self->iterator(); }
void			label_register_observer (Label * self, LabelObserver * o) {
	self->registerObserver(o); }
void 			label_unregister_observer (Label * self, LabelObserver * o) {
	self->unregisterObserver(o); }


LabelList * 	label_list_new (const gchar * name) { return new LabelList (name); }
gboolean		label_list_add (LabelList * self, int number, const gchar * name) {
	return self->add (number, name);
}

LabelFunc * 	label_func_new (const gchar * name, LabelRawFunc func,
								LabelMaxLenFunc maxlen_func /*optional*/,
							    gpointer user_data, GFreeFunc user_dtor)
{
	return new LabelFunc (name, func, maxlen_func, user_data, user_dtor);
}


gboolean		label_iter_is_valid (LabelIter * self) { return self->isValid(); }
void			label_iter_next (LabelIter * self) { self->next(); }
const gchar * 	label_iter_name (LabelIter * self, GString * s) { return self->name(s); }
int				label_iter_number (LabelIter * self) { return self->number(); }
void			label_iter_seek (LabelIter * self, int n) { return self->seek(n); }
void			label_iter_destroy (LabelIter * self) { delete self; }
