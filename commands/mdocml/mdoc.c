/*	$Id: mdoc.c,v 1.146 2010/06/12 11:58:22 kristaps Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "libmdoc.h"
#include "libmandoc.h"

const	char *const __mdoc_macronames[MDOC_MAX] = {		 
	"Ap",		"Dd",		"Dt",		"Os",
	"Sh",		"Ss",		"Pp",		"D1",
	"Dl",		"Bd",		"Ed",		"Bl",
	"El",		"It",		"Ad",		"An",
	"Ar",		"Cd",		"Cm",		"Dv",
	"Er",		"Ev",		"Ex",		"Fa",
	"Fd",		"Fl",		"Fn",		"Ft",
	"Ic",		"In",		"Li",		"Nd",
	"Nm",		"Op",		"Ot",		"Pa",
	"Rv",		"St",		"Va",		"Vt",
	/* LINTED */
	"Xr",		"%A",		"%B",		"%D",
	/* LINTED */
	"%I",		"%J",		"%N",		"%O",
	/* LINTED */
	"%P",		"%R",		"%T",		"%V",
	"Ac",		"Ao",		"Aq",		"At",
	"Bc",		"Bf",		"Bo",		"Bq",
	"Bsx",		"Bx",		"Db",		"Dc",
	"Do",		"Dq",		"Ec",		"Ef",
	"Em",		"Eo",		"Fx",		"Ms",
	"No",		"Ns",		"Nx",		"Ox",
	"Pc",		"Pf",		"Po",		"Pq",
	"Qc",		"Ql",		"Qo",		"Qq",
	"Re",		"Rs",		"Sc",		"So",
	"Sq",		"Sm",		"Sx",		"Sy",
	"Tn",		"Ux",		"Xc",		"Xo",
	"Fo",		"Fc",		"Oo",		"Oc",
	"Bk",		"Ek",		"Bt",		"Hf",
	"Fr",		"Ud",		"Lb",		"Lp",
	"Lk",		"Mt",		"Brq",		"Bro",
	/* LINTED */
	"Brc",		"%C",		"Es",		"En",
	/* LINTED */
	"Dx",		"%Q",		"br",		"sp",
	/* LINTED */
	"%U",		"Ta"
	};

const	char *const __mdoc_argnames[MDOC_ARG_MAX] = {		 
	"split",		"nosplit",		"ragged",
	"unfilled",		"literal",		"file",		 
	"offset",		"bullet",		"dash",		 
	"hyphen",		"item",			"enum",		 
	"tag",			"diag",			"hang",		 
	"ohang",		"inset",		"column",	 
	"width",		"compact",		"std",	 
	"filled",		"words",		"emphasis",
	"symbolic",		"nested",		"centered"
	};

const	char * const *mdoc_macronames = __mdoc_macronames;
const	char * const *mdoc_argnames = __mdoc_argnames;

static	void		  mdoc_node_free(struct mdoc_node *);
static	void		  mdoc_node_unlink(struct mdoc *, 
				struct mdoc_node *);
static	void		  mdoc_free1(struct mdoc *);
static	void		  mdoc_alloc1(struct mdoc *);
static	struct mdoc_node *node_alloc(struct mdoc *, int, int, 
				enum mdoct, enum mdoc_type);
static	int		  node_append(struct mdoc *, 
				struct mdoc_node *);
static	int		  mdoc_ptext(struct mdoc *, int, char *, int);
static	int		  mdoc_pmacro(struct mdoc *, int, char *, int);
static	int		  macrowarn(struct mdoc *, int, 
				const char *, int);


const struct mdoc_node *
mdoc_node(const struct mdoc *m)
{

	return(MDOC_HALT & m->flags ? NULL : m->first);
}


const struct mdoc_meta *
mdoc_meta(const struct mdoc *m)
{

	return(MDOC_HALT & m->flags ? NULL : &m->meta);
}


/*
 * Frees volatile resources (parse tree, meta-data, fields).
 */
static void
mdoc_free1(struct mdoc *mdoc)
{

	if (mdoc->first)
		mdoc_node_delete(mdoc, mdoc->first);
	if (mdoc->meta.title)
		free(mdoc->meta.title);
	if (mdoc->meta.os)
		free(mdoc->meta.os);
	if (mdoc->meta.name)
		free(mdoc->meta.name);
	if (mdoc->meta.arch)
		free(mdoc->meta.arch);
	if (mdoc->meta.vol)
		free(mdoc->meta.vol);
	if (mdoc->meta.msec)
		free(mdoc->meta.msec);
}


/*
 * Allocate all volatile resources (parse tree, meta-data, fields).
 */
static void
mdoc_alloc1(struct mdoc *mdoc)
{

	memset(&mdoc->meta, 0, sizeof(struct mdoc_meta));
	mdoc->flags = 0;
	mdoc->lastnamed = mdoc->lastsec = SEC_NONE;
	mdoc->last = mandoc_calloc(1, sizeof(struct mdoc_node));
	mdoc->first = mdoc->last;
	mdoc->last->type = MDOC_ROOT;
	mdoc->next = MDOC_NEXT_CHILD;
}


/*
 * Free up volatile resources (see mdoc_free1()) then re-initialises the
 * data with mdoc_alloc1().  After invocation, parse data has been reset
 * and the parser is ready for re-invocation on a new tree; however,
 * cross-parse non-volatile data is kept intact.
 */
void
mdoc_reset(struct mdoc *mdoc)
{

	mdoc_free1(mdoc);
	mdoc_alloc1(mdoc);
}


/*
 * Completely free up all volatile and non-volatile parse resources.
 * After invocation, the pointer is no longer usable.
 */
void
mdoc_free(struct mdoc *mdoc)
{

	mdoc_free1(mdoc);
	free(mdoc);
}


/*
 * Allocate volatile and non-volatile parse resources.  
 */
struct mdoc *
mdoc_alloc(void *data, int pflags, mandocmsg msg)
{
	struct mdoc	*p;

	p = mandoc_calloc(1, sizeof(struct mdoc));

	p->msg = msg;
	p->data = data;
	p->pflags = pflags;

	mdoc_hash_init();
	mdoc_alloc1(p);
	return(p);
}


/*
 * Climb back up the parse tree, validating open scopes.  Mostly calls
 * through to macro_end() in macro.c.
 */
int
mdoc_endparse(struct mdoc *m)
{

	if (MDOC_HALT & m->flags)
		return(0);
	else if (mdoc_macroend(m))
		return(1);
	m->flags |= MDOC_HALT;
	return(0);
}


/*
 * Main parse routine.  Parses a single line -- really just hands off to
 * the macro (mdoc_pmacro()) or text parser (mdoc_ptext()).
 */
int
mdoc_parseln(struct mdoc *m, int ln, char *buf, int offs)
{

	if (MDOC_HALT & m->flags)
		return(0);

	m->flags |= MDOC_NEWLINE;
	return(('.' == buf[offs] || '\'' == buf[offs]) ? 
			mdoc_pmacro(m, ln, buf, offs) :
			mdoc_ptext(m, ln, buf, offs));
}


int
mdoc_vmsg(struct mdoc *mdoc, enum mandocerr t, 
		int ln, int pos, const char *fmt, ...)
{
	char		 buf[256];
	va_list		 ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
	va_end(ap);

	return((*mdoc->msg)(t, mdoc->data, ln, pos, buf));
}


int
mdoc_macro(struct mdoc *m, enum mdoct tok, 
		int ln, int pp, int *pos, char *buf)
{
	assert(tok < MDOC_MAX);

	/* If we're in the body, deny prologue calls. */

	if (MDOC_PROLOGUE & mdoc_macros[tok].flags && 
			MDOC_PBODY & m->flags)
		return(mdoc_pmsg(m, ln, pp, MANDOCERR_BADBODY));

	/* If we're in the prologue, deny "body" macros.  */

	if ( ! (MDOC_PROLOGUE & mdoc_macros[tok].flags) && 
			! (MDOC_PBODY & m->flags)) {
		if ( ! mdoc_pmsg(m, ln, pp, MANDOCERR_BADPROLOG))
			return(0);
		if (NULL == m->meta.title)
			m->meta.title = mandoc_strdup("UNKNOWN");
		if (NULL == m->meta.vol)
			m->meta.vol = mandoc_strdup("LOCAL");
		if (NULL == m->meta.os)
			m->meta.os = mandoc_strdup("LOCAL");
		if (0 == m->meta.date)
			m->meta.date = time(NULL);
		m->flags |= MDOC_PBODY;
	}

	return((*mdoc_macros[tok].fp)(m, tok, ln, pp, pos, buf));
}


static int
node_append(struct mdoc *mdoc, struct mdoc_node *p)
{

	assert(mdoc->last);
	assert(mdoc->first);
	assert(MDOC_ROOT != p->type);

	switch (mdoc->next) {
	case (MDOC_NEXT_SIBLING):
		mdoc->last->next = p;
		p->prev = mdoc->last;
		p->parent = mdoc->last->parent;
		break;
	case (MDOC_NEXT_CHILD):
		mdoc->last->child = p;
		p->parent = mdoc->last;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	p->parent->nchild++;

	if ( ! mdoc_valid_pre(mdoc, p))
		return(0);
	if ( ! mdoc_action_pre(mdoc, p))
		return(0);

	switch (p->type) {
	case (MDOC_HEAD):
		assert(MDOC_BLOCK == p->parent->type);
		p->parent->head = p;
		break;
	case (MDOC_TAIL):
		assert(MDOC_BLOCK == p->parent->type);
		p->parent->tail = p;
		break;
	case (MDOC_BODY):
		assert(MDOC_BLOCK == p->parent->type);
		p->parent->body = p;
		break;
	default:
		break;
	}

	mdoc->last = p;

	switch (p->type) {
	case (MDOC_TEXT):
		if ( ! mdoc_valid_post(mdoc))
			return(0);
		if ( ! mdoc_action_post(mdoc))
			return(0);
		break;
	default:
		break;
	}

	return(1);
}


static struct mdoc_node *
node_alloc(struct mdoc *m, int line, int pos, 
		enum mdoct tok, enum mdoc_type type)
{
	struct mdoc_node *p;

	p = mandoc_calloc(1, sizeof(struct mdoc_node));
	p->sec = m->lastsec;
	p->line = line;
	p->pos = pos;
	p->tok = tok;
	p->type = type;
	if (MDOC_NEWLINE & m->flags)
		p->flags |= MDOC_LINE;
	m->flags &= ~MDOC_NEWLINE;
	return(p);
}


int
mdoc_tail_alloc(struct mdoc *m, int line, int pos, enum mdoct tok)
{
	struct mdoc_node *p;

	p = node_alloc(m, line, pos, tok, MDOC_TAIL);
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_head_alloc(struct mdoc *m, int line, int pos, enum mdoct tok)
{
	struct mdoc_node *p;

	assert(m->first);
	assert(m->last);

	p = node_alloc(m, line, pos, tok, MDOC_HEAD);
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_body_alloc(struct mdoc *m, int line, int pos, enum mdoct tok)
{
	struct mdoc_node *p;

	p = node_alloc(m, line, pos, tok, MDOC_BODY);
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_block_alloc(struct mdoc *m, int line, int pos, 
		enum mdoct tok, struct mdoc_arg *args)
{
	struct mdoc_node *p;

	p = node_alloc(m, line, pos, tok, MDOC_BLOCK);
	p->args = args;
	if (p->args)
		(args->refcnt)++;
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_elem_alloc(struct mdoc *m, int line, int pos, 
		enum mdoct tok, struct mdoc_arg *args)
{
	struct mdoc_node *p;

	p = node_alloc(m, line, pos, tok, MDOC_ELEM);
	p->args = args;
	if (p->args)
		(args->refcnt)++;
	if ( ! node_append(m, p))
		return(0);
	m->next = MDOC_NEXT_CHILD;
	return(1);
}


int
mdoc_word_alloc(struct mdoc *m, int line, int pos, const char *p)
{
	struct mdoc_node *n;
	size_t		  sv, len;

	len = strlen(p);

	n = node_alloc(m, line, pos, MDOC_MAX, MDOC_TEXT);
	n->string = mandoc_malloc(len + 1);
	sv = strlcpy(n->string, p, len + 1);

	/* Prohibit truncation. */
	assert(sv < len + 1);

	if ( ! node_append(m, n))
		return(0);

	m->next = MDOC_NEXT_SIBLING;
	return(1);
}


static void
mdoc_node_free(struct mdoc_node *p)
{

	if (p->string)
		free(p->string);
	if (p->args)
		mdoc_argv_free(p->args);
	free(p);
}


static void
mdoc_node_unlink(struct mdoc *m, struct mdoc_node *n)
{

	/* Adjust siblings. */

	if (n->prev)
		n->prev->next = n->next;
	if (n->next)
		n->next->prev = n->prev;

	/* Adjust parent. */

	if (n->parent) {
		n->parent->nchild--;
		if (n->parent->child == n)
			n->parent->child = n->prev ? n->prev : n->next;
	}

	/* Adjust parse point, if applicable. */

	if (m && m->last == n) {
		if (n->prev) {
			m->last = n->prev;
			m->next = MDOC_NEXT_SIBLING;
		} else {
			m->last = n->parent;
			m->next = MDOC_NEXT_CHILD;
		}
	}

	if (m && m->first == n)
		m->first = NULL;
}


void
mdoc_node_delete(struct mdoc *m, struct mdoc_node *p)
{

	while (p->child) {
		assert(p->nchild);
		mdoc_node_delete(m, p->child);
	}
	assert(0 == p->nchild);

	mdoc_node_unlink(m, p);
	mdoc_node_free(p);
}


/*
 * Parse free-form text, that is, a line that does not begin with the
 * control character.
 */
static int
mdoc_ptext(struct mdoc *m, int line, char *buf, int offs)
{
	char		 *c, *ws, *end;
	struct mdoc_node *n;

	/* Ignore bogus comments. */

	if ('\\' == buf[offs] && 
			'.' == buf[offs + 1] && 
			'"' == buf[offs + 2])
		return(mdoc_pmsg(m, line, offs, MANDOCERR_BADCOMMENT));

	/* No text before an initial macro. */

	if (SEC_NONE == m->lastnamed)
		return(mdoc_pmsg(m, line, offs, MANDOCERR_NOTEXT));

	assert(m->last);
	n = m->last;

	/*
	 * Divert directly to list processing if we're encountering a
	 * columnar MDOC_BLOCK with or without a prior MDOC_BLOCK entry
	 * (a MDOC_BODY means it's already open, in which case we should
	 * process within its context in the normal way).
	 */

	if (MDOC_Bl == n->tok && MDOC_BODY == n->type &&
			LIST_column == n->data.Bl.type) {
		/* `Bl' is open without any children. */
		m->flags |= MDOC_FREECOL;
		return(mdoc_macro(m, MDOC_It, line, offs, &offs, buf));
	}

	if (MDOC_It == n->tok && MDOC_BLOCK == n->type &&
			NULL != n->parent &&
			MDOC_Bl == n->parent->tok &&
			LIST_column == n->parent->data.Bl.type) {
		/* `Bl' has block-level `It' children. */
		m->flags |= MDOC_FREECOL;
		return(mdoc_macro(m, MDOC_It, line, offs, &offs, buf));
	}

	/*
	 * Search for the beginning of unescaped trailing whitespace (ws)
	 * and for the first character not to be output (end).
	 */

	/* FIXME: replace with strcspn(). */
	ws = NULL;
	for (c = end = buf + offs; *c; c++) {
		switch (*c) {
		case '-':
			if (mandoc_hyph(buf + offs, c))
				*c = ASCII_HYPH;
			ws = NULL;
			break;
		case ' ':
			if (NULL == ws)
				ws = c;
			continue;
		case '\t':
			/*
			 * Always warn about trailing tabs,
			 * even outside literal context,
			 * where they should be put on the next line.
			 */
			if (NULL == ws)
				ws = c;
			/*
			 * Strip trailing tabs in literal context only;
			 * outside, they affect the next line.
			 */
			if (MDOC_LITERAL & m->flags)
				continue;
			break;
		case '\\':
			/* Skip the escaped character, too, if any. */
			if (c[1])
				c++;
			/* FALLTHROUGH */
		default:
			ws = NULL;
			break;
		}
		end = c + 1;
	}
	*end = '\0';

	if (ws)
		if ( ! mdoc_pmsg(m, line, (int)(ws-buf), MANDOCERR_EOLNSPACE))
			return(0);

	if ('\0' == buf[offs] && ! (MDOC_LITERAL & m->flags)) {
		if ( ! mdoc_pmsg(m, line, (int)(c-buf), MANDOCERR_NOBLANKLN))
			return(0);

		/*
		 * Insert a `Pp' in the case of a blank line.  Technically,
		 * blank lines aren't allowed, but enough manuals assume this
		 * behaviour that we want to work around it.
		 */
		if ( ! mdoc_elem_alloc(m, line, offs, MDOC_Pp, NULL))
			return(0);

		m->next = MDOC_NEXT_SIBLING;
		return(1);
	}

	if ( ! mdoc_word_alloc(m, line, offs, buf+offs))
		return(0);

	if (MDOC_LITERAL & m->flags)
		return(1);

	/*
	 * End-of-sentence check.  If the last character is an unescaped
	 * EOS character, then flag the node as being the end of a
	 * sentence.  The front-end will know how to interpret this.
	 */

	assert(buf < end);

	if (mandoc_eos(buf+offs, (size_t)(end-buf-offs)))
		m->last->flags |= MDOC_EOS;

	return(1);
}


static int
macrowarn(struct mdoc *m, int ln, const char *buf, int offs)
{
	int		 rc;

	rc = mdoc_vmsg(m, MANDOCERR_MACRO, ln, offs, 
			"unknown macro: %s%s", 
			buf, strlen(buf) > 3 ? "..." : "");

	/* FIXME: logic should be in driver. */
	/* FIXME: broken, will error out and not omit a message. */
	return(MDOC_IGN_MACRO & m->pflags ? rc : 0);
}


/*
 * Parse a macro line, that is, a line beginning with the control
 * character.
 */
static int
mdoc_pmacro(struct mdoc *m, int ln, char *buf, int offs)
{
	enum mdoct	  tok;
	int		  i, j, sv;
	char		  mac[5];
	struct mdoc_node *n;

	/* Empty lines are ignored. */

	offs++;

	if ('\0' == buf[offs])
		return(1);

	i = offs;

	/* Accept whitespace after the initial control char. */

	if (' ' == buf[i]) {
		i++;
		while (buf[i] && ' ' == buf[i])
			i++;
		if ('\0' == buf[i])
			return(1);
	}

	sv = i;

	/* Copy the first word into a nil-terminated buffer. */

	for (j = 0; j < 4; j++, i++) {
		if ('\0' == (mac[j] = buf[i]))
			break;
		else if (' ' == buf[i])
			break;

		/* Check for invalid characters. */

		if (isgraph((u_char)buf[i]))
			continue;
		if ( ! mdoc_pmsg(m, ln, i, MANDOCERR_BADCHAR))
			return(0);
		i--;
	}

	mac[j] = '\0';

	if (j == 4 || j < 2) {
		if ( ! macrowarn(m, ln, mac, sv))
			goto err;
		return(1);
	} 
	
	if (MDOC_MAX == (tok = mdoc_hash_find(mac))) {
		if ( ! macrowarn(m, ln, mac, sv))
			goto err;
		return(1);
	}

	/* The macro is sane.  Jump to the next word. */

	while (buf[i] && ' ' == buf[i])
		i++;

	/* 
	 * Trailing whitespace.  Note that tabs are allowed to be passed
	 * into the parser as "text", so we only warn about spaces here.
	 */

	if ('\0' == buf[i] && ' ' == buf[i - 1])
		if ( ! mdoc_pmsg(m, ln, i - 1, MANDOCERR_EOLNSPACE))
			goto err;

	/*
	 * If an initial macro or a list invocation, divert directly
	 * into macro processing.
	 */

	if (NULL == m->last || MDOC_It == tok || MDOC_El == tok) {
		if ( ! mdoc_macro(m, tok, ln, sv, &i, buf)) 
			goto err;
		return(1);
	}

	n = m->last;
	assert(m->last);

	/*
	 * If the first macro of a `Bl -column', open an `It' block
	 * context around the parsed macro.
	 */

	if (MDOC_Bl == n->tok && MDOC_BODY == n->type &&
			LIST_column == n->data.Bl.type) {
		m->flags |= MDOC_FREECOL;
		if ( ! mdoc_macro(m, MDOC_It, ln, sv, &sv, buf)) 
			goto err;
		return(1);
	}

	/*
	 * If we're following a block-level `It' within a `Bl -column'
	 * context (perhaps opened in the above block or in ptext()),
	 * then open an `It' block context around the parsed macro.
	 */

	if (MDOC_It == n->tok && MDOC_BLOCK == n->type &&
			NULL != n->parent &&
			MDOC_Bl == n->parent->tok &&
			LIST_column == n->parent->data.Bl.type) {
		m->flags |= MDOC_FREECOL;
		if ( ! mdoc_macro(m, MDOC_It, ln, sv, &sv, buf)) 
			goto err;
		return(1);
	}

	/* Normal processing of a macro. */

	if ( ! mdoc_macro(m, tok, ln, sv, &i, buf)) 
		goto err;

	return(1);

err:	/* Error out. */

	m->flags |= MDOC_HALT;
	return(0);
}


