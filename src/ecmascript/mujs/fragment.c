/* The MuJS html DocumentFragment objects implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "cookies/cookies.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/libdom/corestrings.h"
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#include "document/libdom/renderer2.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/mujs/mapa.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/attr.h"
#include "ecmascript/mujs/attributes.h"
#include "ecmascript/mujs/collection.h"
#include "ecmascript/mujs/dataset.h"
#include "ecmascript/mujs/document.h"
#include "ecmascript/mujs/domrect.h"
#include "ecmascript/mujs/element.h"
#include "ecmascript/mujs/event.h"
#include "ecmascript/mujs/fragment.h"
#include "ecmascript/mujs/keyboard.h"
#include "ecmascript/mujs/nodelist.h"
#include "ecmascript/mujs/nodelist2.h"
#include "ecmascript/mujs/style.h"
#include "ecmascript/mujs/tokenlist.h"
#include "ecmascript/mujs/window.h"
#include "intl/libintl.h"
#include "main/select.h"
#include "osdep/newwin.h"
#include "osdep/sysname.h"
#include "protocol/http/http.h"
#include "protocol/uri.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/vs.h"

struct fragment_listener {
	LIST_HEAD_EL(struct fragment_listener);
	char *typ;
	const char *fun;
};

struct mjs_fragment_private {
	struct ecmascript_interpreter *interpreter;
	const char *thisval;
	LIST_OF(struct fragment_listener) listeners;
	dom_event_listener *listener;
	void *node;
	int ref_count;
};

static void fragment_event_handler(dom_event *event, void *pw);
static void mjs_fragment_dispatchEvent(js_State *J);
static void mjs_fragment_set_property_textContent(js_State *J);

void *
mjs_getprivate_fragment(js_State *J, int idx)
{
	struct mjs_fragment_private *priv = (struct mjs_fragment_private *)js_touserdata(J, idx, "fragment");

	if (!priv) {
		return NULL;
	}

	return priv->node;
}

#if 0
static void
mjs_fragment_get_property_attributes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_namednodemap *attrs = NULL;
	dom_exception exc = dom_node_get_attributes(el, &attrs);

	if (exc != DOM_NO_ERR || !attrs) {
		js_pushnull(J);
		return;
	}
	mjs_push_attributes(J, attrs);
}
#endif

#if 0
static void
mjs_fragment_get_property_checked(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	struct el_form_control *fc;
	struct form_state *fs;
	struct link *link;
	int offset, linknum;

	if (!vs) {
		js_pushundefined(J);
		return;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	doc = doc_view->document;

	if (!el) {
		js_pushundefined(J);
		return;
	}
	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		js_pushundefined(J);
		return;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		js_pushundefined(J);
		return;
	}

	link = &doc->links[linknum];
	fc = get_link_form_control(link);

	if (!fc) {
		js_pushundefined(J);
		return;
	}
	fs = find_form_state(doc_view, fc);

	if (!fs) {
		js_pushundefined(J);
		return;
	}
	js_pushboolean(J, fs->state);
}
#endif

static void
mjs_fragment_get_property_children(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	mjs_push_nodelist(J, nodes);
	dom_nodelist_unref(nodes);
}

static void
mjs_fragment_get_property_childElementCount(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t res = 0;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	exc = dom_nodelist_get_length(nodes, &res);
	dom_nodelist_unref(nodes);
	js_pushnumber(J, res);
}

static void
mjs_fragment_get_property_childNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	mjs_push_nodelist(J, nodes);
	dom_nodelist_unref(nodes);
}

#if 0
static void
mjs_fragment_get_property_classList(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_element *el = (dom_element *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_ref(el);
	dom_tokenlist *tl = NULL;
	dom_exception exc = dom_tokenlist_create(el, corestring_dom_class, &tl);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(el);

	if (exc != DOM_NO_ERR || !tl) {
		js_pushnull(J);
		return;
	}
	mjs_push_tokenlist(J, tl);
}
#endif

#if 0
static void
mjs_fragment_get_property_className(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_html_element *el = (dom_html_element *)(mjs_getprivate_fragment(J, 0));
	dom_string *classstr = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_html_element_get_class_name(el, &classstr);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!classstr) {
		js_pushstring(J, "");
		return;
	} else {
		js_pushstring(J, dom_string_data(classstr));
		dom_string_unref(classstr);
	}
}
#endif

#if 0
static void
mjs_fragment_get_property_clientHeight(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		js_pushnumber(J, 0);
		return;
	}
	ses = doc_view->session;

	if (!ses) {
		js_pushnumber(J, 0);
		return;
	}
	dom_string *tag_name = NULL;
	dom_exception exc = dom_node_get_node_name(el, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		js_pushnumber(J, 0);
		return;
	}
	bool root = (!strcmp(dom_string_data(tag_name), "BODY") || !strcmp(dom_string_data(tag_name), "HTML"));
	dom_string_unref(tag_name);

	if (root) {
		int height = doc_view->box.height * ses->tab->term->cell_height;
		js_pushnumber(J, height);
		return;
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		js_pushnumber(J, 0);
		return;
	}
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		js_pushnumber(J, 0);
		return;
	}
	int dy = int_max(0, (rect->y1 + 1 - rect->y0) * ses->tab->term->cell_height);
	js_pushnumber(J, dy);
}
#endif

#if 0
static void
mjs_fragment_get_property_clientLeft(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, 0);
}
#endif

#if 0
static void
mjs_fragment_get_property_clientTop(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushnumber(J, 0);
}
#endif

#if 0
static void
mjs_fragment_get_property_clientWidth(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		js_pushnumber(J, 0);
		return;
	}
	ses = doc_view->session;

	if (!ses) {
		js_pushnumber(J, 0);
		return;
	}
	dom_string *tag_name = NULL;
	dom_exception exc = dom_node_get_node_name(el, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		js_pushnumber(J, 0);
		return;
	}
	bool root = (!strcmp(dom_string_data(tag_name), "BODY") || !strcmp(dom_string_data(tag_name), "HTML") || !strcmp(dom_string_data(tag_name), "DIV"));
	dom_string_unref(tag_name);

	if (root) {
		int width = doc_view->box.width * ses->tab->term->cell_width;
		js_pushnumber(J, width);
		return;
	}

	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		js_pushnumber(J, 0);
		return;
	}
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		js_pushnumber(J, 0);
		return;
	}
	int dx = int_max(0, (rect->x1 + 1 - rect->x0) * ses->tab->term->cell_width);
	js_pushnumber(J, dx);
}
#endif

#if 0
static void
mjs_fragment_get_property_dataset(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_element *el = (dom_element *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	mjs_push_dataset(J, el);
}
#endif

#if 0
static void
mjs_fragment_get_property_dir(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_string *dir = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_dir, &dir);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!dir) {
		js_pushstring(J, "");
		return;
	} else {
		if (strcmp(dom_string_data(dir), "auto") && strcmp(dom_string_data(dir), "ltr") && strcmp(dom_string_data(dir), "rtl")) {
			js_pushstring(J, "");
		} else {
			js_pushstring(J, dom_string_data(dir));
		}
		dom_string_unref(dir);
	}
}
#endif

static void
mjs_fragment_get_property_firstChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_first_child(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

static void
mjs_fragment_get_property_firstElementChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t size = 0;
	uint32_t i;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	exc = dom_nodelist_get_length(nodes, &size);

	if (exc != DOM_NO_ERR || !size) {
		dom_nodelist_unref(nodes);
		js_pushnull(J);
		return;
	}

	for (i = 0; i < size; i++) {
		dom_node *child = NULL;
		exc = dom_nodelist_item(nodes, i, &child);
		dom_node_type type;

		if (exc != DOM_NO_ERR || !child) {
			continue;
		}

		exc = dom_node_get_node_type(child, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dom_nodelist_unref(nodes);
			mjs_push_element(J, child);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(child);
			return;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	js_pushnull(J);
}

#if 0
static void
mjs_fragment_get_property_href(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_string *href = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_href, &href);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!href) {
		js_pushstring(J, "");
		return;
	} else {
		js_pushstring(J, dom_string_data(href));
		dom_string_unref(href);
	}
}
#endif

static void
mjs_fragment_get_property_id(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_string *id = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_id, &id);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!id) {
		js_pushstring(J, "");
		return;
	} else {
		js_pushstring(J, dom_string_data(id));
		dom_string_unref(id);
	}
}

#if 0
static void
mjs_fragment_get_property_lang(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_string *lang = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_lang, &lang);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!lang) {
		js_pushstring(J, "");
	} else {
		js_pushstring(J, dom_string_data(lang));
		dom_string_unref(lang);
	}
}
#endif

static void
mjs_fragment_get_property_lastChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *last_child = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_last_child(el, &last_child);

	if (exc != DOM_NO_ERR || !last_child) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, last_child);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(last_child);
}

static void
mjs_fragment_get_property_lastElementChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_nodelist *nodes = NULL;
	dom_exception exc;
	uint32_t size = 0;
	int i;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_child_nodes(el, &nodes);

	if (exc != DOM_NO_ERR || !nodes) {
		js_pushnull(J);
		return;
	}
	exc = dom_nodelist_get_length(nodes, &size);

	if (exc != DOM_NO_ERR || !size) {
		dom_nodelist_unref(nodes);
		js_pushnull(J);
		return;
	}

	for (i = size - 1; i >= 0 ; i--) {
		dom_node *child = NULL;
		exc = dom_nodelist_item(nodes, i, &child);
		dom_node_type type;

		if (exc != DOM_NO_ERR || !child) {
			continue;
		}
		exc = dom_node_get_node_type(child, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			dom_nodelist_unref(nodes);
			mjs_push_element(J, child);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(child);
			return;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
	}
	dom_nodelist_unref(nodes);
	js_pushnull(J);
}

#if 0
static void
mjs_fragment_get_property_nextElementSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *node;
	dom_node *prev_next = NULL;

	if (!el) {
		js_pushnull(J);
		return;
	}
	node = el;

	while (true) {
		dom_node *next = NULL;
		dom_exception exc = dom_node_get_next_sibling(node, &next);
		dom_node_type type;

		if (prev_next) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(prev_next);
		}

		if (exc != DOM_NO_ERR || !next) {
			js_pushnull(J);
			return;
		}
		exc = dom_node_get_node_type(next, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			mjs_push_element(J, next);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(next);
			return;
		}
		prev_next = next;
		node = next;
	}
	js_pushnull(J);
}
#endif

static void
mjs_fragment_get_property_nodeName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_string *name = NULL;
	dom_exception exc;

	if (!node) {
		js_pushstring(J, "");
		return;
	}
	exc = dom_node_get_node_name(node, &name);

	if (exc != DOM_NO_ERR || !name) {
		js_pushstring(J, "");
		return;
	}
	js_pushstring(J, dom_string_data(name));
	dom_string_unref(name);
}

static void
mjs_fragment_get_property_nodeType(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node_type type;
	dom_exception exc;

	if (!node) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_node_type(node, &type);

	if (exc == DOM_NO_ERR) {
		js_pushnumber(J, type);
		return;
	}
	js_pushnull(J);
}

static void
mjs_fragment_get_property_nodeValue(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *node = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_string *content = NULL;
	dom_exception exc;

	if (!node) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_node_value(node, &content);

	if (exc != DOM_NO_ERR || !content) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(content));
	dom_string_unref(content);
}

#if 0
static void
mjs_fragment_get_property_nextSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_next_sibling(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}
#endif

#if 0
static void
mjs_fragment_get_property_offsetHeight(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_fragment_get_property_clientHeight(J);
}
#endif

#if 0
static void
mjs_fragment_get_property_offsetLeft(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		js_pushnumber(J, 0);
		return;
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		js_pushnumber(J, 0);
		return;
	}
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		js_pushnumber(J, 0);
		return;
	}
	ses = doc_view->session;

	if (!ses) {
		js_pushnumber(J, 0);
		return;
	}
	dom_node *node = NULL;
	dom_exception exc = dom_node_get_parent_node(el, &node);
	if (exc != DOM_NO_ERR || !node) {
		js_pushnumber(J, 0);
		return;
	}
	int offset_parent = find_offset(document->element_map_rev, node);

	if (offset_parent <= 0) {
		js_pushnumber(J, 0);
		return;
	}
	struct node_rect *rect_parent = get_element_rect(document, offset_parent);

	if (!rect_parent) {
		js_pushnumber(J, 0);
		return;
	}
	int dx = int_max(0, (rect->x0 - rect_parent->x0) * ses->tab->term->cell_width);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
	js_pushnumber(J, dx);
}
#endif

#if 0
static void
mjs_fragment_get_property_offsetParent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}
#endif

#if 0
static void
mjs_fragment_get_property_offsetTop(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view = vs->doc_view;
	struct document *document = doc_view->document;
	struct session *ses;

	if (!document) {
		js_pushnumber(J, 0);
		return;
	}
	int offset = find_offset(document->element_map_rev, el);

	if (offset <= 0) {
		js_pushnumber(J, 0);
		return;
	}
	struct node_rect *rect = get_element_rect(document, offset);

	if (!rect) {
		js_pushnumber(J, 0);
		return;
	}
	ses = doc_view->session;

	if (!ses) {
		js_pushnumber(J, 0);
		return;
	}
	dom_node *node = NULL;
	dom_exception exc = dom_node_get_parent_node(el, &node);
	if (exc != DOM_NO_ERR || !node) {
		js_pushnumber(J, 0);
		return;
	}
	int offset_parent = find_offset(document->element_map_rev, node);

	if (offset_parent <= 0) {
		js_pushnumber(J, 0);
		return;
	}
	struct node_rect *rect_parent = get_element_rect(document, offset_parent);

	if (!rect_parent) {
		js_pushnumber(J, 0);
		return;
	}
	int dy = int_max(0, (rect->y0 - rect_parent->y0) * ses->tab->term->cell_height);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
	js_pushnumber(J, dy);
}
#endif

#if 0
static void
mjs_fragment_get_property_offsetWidth(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_fragment_get_property_clientWidth(J);
}
#endif

static void
mjs_fragment_get_property_ownerDocument(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	mjs_push_document(J, interpreter);
}

static void
mjs_fragment_get_property_parentElement(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

static void
mjs_fragment_get_property_parentNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_parent_node(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

static void
mjs_fragment_get_property_previousElementSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *node;
	dom_node *prev_prev = NULL;

	if (!el) {
		js_pushnull(J);
		return;
	}
	node = el;

	while (true) {
		dom_node *prev = NULL;
		dom_exception exc = dom_node_get_previous_sibling(node, &prev);
		dom_node_type type;

		if (prev_prev) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(prev_prev);
		}

		if (exc != DOM_NO_ERR || !prev) {
			js_pushnull(J);
			return;
		}
		exc = dom_node_get_node_type(prev, &type);

		if (exc == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			mjs_push_element(J, prev);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(prev);
			return;
		}
		prev_prev = prev;
		node = prev;
	}
	js_pushnull(J);
}

static void
mjs_fragment_get_property_previousSibling(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *node = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_node_get_previous_sibling(el, &node);

	if (exc != DOM_NO_ERR || !node) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, node);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(node);
}

#if 0
static void
mjs_fragment_get_property_style(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	mjs_push_style(J, el);
}
#endif

#if 0
static void
mjs_fragment_get_property_tagName(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_string *tag_name = NULL;
	dom_exception exc = dom_node_get_node_name(el, &tag_name);

	if (exc != DOM_NO_ERR || !tag_name) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(tag_name));
	dom_string_unref(tag_name);
}
#endif

#if 0
static void
mjs_fragment_get_property_title(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_string *title = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	exc = dom_element_get_attribute(el, corestring_dom_title, &title);

	if (exc != DOM_NO_ERR) {
		js_pushnull(J);
		return;
	}
	if (!title) {
		js_pushstring(J, "");
	} else {
		js_pushstring(J, dom_string_data(title));
		dom_string_unref(title);
	}
}
#endif

#if 0
static void
mjs_fragment_get_property_value(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	struct el_form_control *fc;
	struct form_state *fs;
	struct link *link;
	int offset, linknum;

	if (!vs) {
		js_pushundefined(J);
		return;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	doc = doc_view->document;

	if (!el) {
		js_pushundefined(J);
		return;
	}
	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		js_pushundefined(J);
		return;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		js_pushundefined(J);
		return;
	}

	link = &doc->links[linknum];
	fc = get_link_form_control(link);

	if (!fc) {
		js_pushundefined(J);
		return;
	}
	fs = find_form_state(doc_view, fc);

	if (!fs) {
		js_pushundefined(J);
		return;
	}
	js_pushstring(J, fs->value);
}
#endif

#if 0
static void
mjs_fragment_get_property_innerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct string buf;
	if (!init_string(&buf)) {
		js_error(J, "out of memory");
		return;
	}
	ecmascript_walk_tree(&buf, el, true, false);
	js_pushstring(J, buf.source);
	done_string(&buf);
}
#endif

#if 0
static void
mjs_fragment_get_property_outerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	struct string buf;
	if (!init_string(&buf)) {
		js_error(J, "out of memory");
		return;
	}
	ecmascript_walk_tree(&buf, el, false, false);
	js_pushstring(J, buf.source);
	done_string(&buf);
}
#endif

static void
mjs_fragment_get_property_textContent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_string *content = NULL;
	dom_exception exc = dom_node_get_text_content(el, &content);

	if (exc != DOM_NO_ERR || !content) {
		js_pushnull(J);
		return;
	}
	js_pushstring(J, dom_string_data(content));
	dom_string_unref(content);
}

#if 0
static void
mjs_fragment_set_property_checked(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	struct el_form_control *fc;
	struct form_state *fs;
	struct link *link;
	int offset, linknum;

	if (!vs) {
		js_pushundefined(J);
		return;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	doc = doc_view->document;

	if (!el) {
		js_pushundefined(J);
		return;
	}
	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		js_pushundefined(J);
		return;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		js_pushundefined(J);
		return;
	}

	link = &doc->links[linknum];
	fc = get_link_form_control(link);

	if (!fc) {
		js_pushundefined(J);
		return;
	}
	fs = find_form_state(doc_view, fc);

	if (!fs) {
		js_pushundefined(J);
		return;
	}

	if (fc->type != FC_CHECKBOX && fc->type != FC_RADIO) {
		js_pushundefined(J);
		return;
	}
	fs->state = js_toboolean(J, 1);
	js_pushundefined(J);
}
#endif

#if 0
static void
mjs_fragment_set_property_className(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_string *classstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_html_element *el = (dom_html_element *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &classstr);

	if (exc == DOM_NO_ERR && classstr) {
		exc = dom_html_element_set_class_name(el, classstr);
		interpreter->changed = 1;
		dom_string_unref(classstr);
	}
	js_pushundefined(J);
}
#endif

#if 0
static void
mjs_fragment_set_property_dir(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	if (!strcmp(str, "ltr") || !strcmp(str, "rtl") || !strcmp(str, "auto")) {
		dom_string *dir = NULL;
		exc = dom_string_create((const uint8_t *)str, strlen(str), &dir);

		if (exc == DOM_NO_ERR && dir) {
			exc = dom_element_set_attribute(el, corestring_dom_dir, dir);
			interpreter->changed = 1;
			dom_string_unref(dir);
		}
	}
	js_pushundefined(J);
}
#endif

#if 0
static void
mjs_fragment_set_property_href(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_string *hrefstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);
	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &hrefstr);

	if (exc == DOM_NO_ERR && hrefstr) {
		exc = dom_element_set_attribute(el, corestring_dom_href, hrefstr);
		interpreter->changed = 1;
		dom_string_unref(hrefstr);
	}
	js_pushundefined(J);
}
#endif

static void
mjs_fragment_set_property_id(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_string *idstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);
	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &idstr);

	if (exc == DOM_NO_ERR && idstr) {
		exc = dom_element_set_attribute(el, corestring_dom_id, idstr);
		interpreter->changed = 1;
		dom_string_unref(idstr);
	}
	js_pushundefined(J);
}

#if 0
static void
mjs_fragment_set_property_innerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *s = js_tostring(J, 1);

	if (!s) {
		js_error(J, "out of memory");
		return;
	}
	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	dom_hubbub_parser *parser = NULL;
	struct dom_document *doc = NULL;
	struct dom_document_fragment *fragment = NULL;
	dom_exception exc;
	struct dom_node *child = NULL, *html = NULL, *body = NULL;
	struct dom_nodelist *bodies = NULL;

	exc = dom_node_get_owner_document(el, &doc);
	if (exc != DOM_NO_ERR) goto out;

	parse_params.enc = "UTF-8";
	parse_params.fix_enc = true;
	parse_params.enable_script = false;
	parse_params.msg = NULL;
	parse_params.script = NULL;
	parse_params.ctx = NULL;
	parse_params.daf = NULL;

	error = dom_hubbub_fragment_parser_create(&parse_params,
						  doc,
						  &parser,
						  &fragment);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to create fragment parser!");
		goto out;
	}

	error = dom_hubbub_parser_parse_chunk(parser, (const uint8_t*)s, strlen(s));
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to parse HTML chunk");
		goto out;
	}
	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to complete parser");
		goto out;
	}

	/* Parse is finished, transfer contents of fragment into node */

	/* 1. empty this node */
	exc = dom_node_get_first_child(el, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		struct dom_node *cref;
		exc = dom_node_remove_child(el, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
		child = NULL;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(cref);
		exc = dom_node_get_first_child(el, &child);
		if (exc != DOM_NO_ERR) goto out;
	}

	/* 2. the first child in the fragment will be an HTML element
	 * because that's how hubbub works, walk through that to the body
	 * element hubbub will have created, we want to migrate that element's
	 * children into ourself.
	 */
	exc = dom_node_get_first_child(fragment, &html);
	if (exc != DOM_NO_ERR) goto out;

	/* We can then ask that HTML element to give us its body */
	exc = dom_element_get_elements_by_tag_name(html, corestring_dom_BODY, &bodies);
	if (exc != DOM_NO_ERR) goto out;

	/* And now we can get the body which will be the zeroth body */
	exc = dom_nodelist_item(bodies, 0, &body);
	if (exc != DOM_NO_ERR) goto out;

	/* 3. Migrate the children */
	exc = dom_node_get_first_child(body, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		struct dom_node *cref;
		exc = dom_node_remove_child(body, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(cref);
		exc = dom_node_append_child(el, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(cref);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
		child = NULL;
		exc = dom_node_get_first_child(body, &child);
		if (exc != DOM_NO_ERR) goto out;
	}
out:
	if (parser != NULL) {
		dom_hubbub_parser_destroy(parser);
	}
	if (doc != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(doc);
	}
	if (fragment != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(fragment);
	}
	if (child != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
	}
	if (html != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(html);
	}
	if (bodies != NULL) {
		dom_nodelist_unref(bodies);
	}
	if (body != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(body);
	}
	interpreter->changed = 1;
	js_pushundefined(J);
}
#endif

#if 0
static void
mjs_fragment_set_property_innerText(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	mjs_fragment_set_property_textContent(J);
}
#endif

#if 0
static void
mjs_fragment_set_property_lang(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_string *langstr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);
	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &langstr);

	if (exc == DOM_NO_ERR && langstr) {
		exc = dom_element_set_attribute(el, corestring_dom_lang, langstr);
		interpreter->changed = 1;
		dom_string_unref(langstr);
	}
	js_pushundefined(J);
}
#endif

#if 0
static void
mjs_fragment_set_property_outerHtml(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	if (!vs) {
		js_pushundefined(J);
		return;
	}
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	dom_node *parent = NULL;
	dom_exception exc = dom_node_get_parent_node(el, &parent);

	if (exc != DOM_NO_ERR) {
		js_pushundefined(J);
		return;
	}
	size_t size;
	const char *s = js_tostring(J, 1);

	if (!s) {
		js_error(J, "out of memory");
		return;
	}
	size = strlen(s);
	struct dom_node *cref = NULL;

	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	dom_hubbub_parser *parser = NULL;
	struct dom_document *doc = NULL;
	struct dom_document_fragment *fragment = NULL;
	struct dom_node *child = NULL, *html = NULL, *body = NULL;
	struct dom_nodelist *bodies = NULL;

	exc = dom_node_get_owner_document(el, &doc);
	if (exc != DOM_NO_ERR) goto out;

	parse_params.enc = "UTF-8";
	parse_params.fix_enc = true;
	parse_params.enable_script = false;
	parse_params.msg = NULL;
	parse_params.script = NULL;
	parse_params.ctx = NULL;
	parse_params.daf = NULL;

	error = dom_hubbub_fragment_parser_create(&parse_params,
						  doc,
						  &parser,
						  &fragment);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to create fragment parser!");
		goto out;
	}

	error = dom_hubbub_parser_parse_chunk(parser, (const uint8_t*)s, size);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to parse HTML chunk");
		goto out;
	}
	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		fprintf(stderr, "Unable to complete parser");
		goto out;
	}

	/* The first child in the fragment will be an HTML element
	 * because that's how hubbub works, walk through that to the body
	 * element hubbub will have created, we want to migrate that element's
	 * children into ourself.
	 */
	exc = dom_node_get_first_child(fragment, &html);
	if (exc != DOM_NO_ERR) goto out;

	/* We can then ask that HTML element to give us its body */
	exc = dom_element_get_elements_by_tag_name(html, corestring_dom_BODY, &bodies);
	if (exc != DOM_NO_ERR) goto out;

	/* And now we can get the body which will be the zeroth body */
	exc = dom_nodelist_item(bodies, 0, &body);
	if (exc != DOM_NO_ERR) goto out;

	/* Migrate the children */
	exc = dom_node_get_first_child(body, &child);
	if (exc != DOM_NO_ERR) goto out;
	while (child != NULL) {
		exc = dom_node_remove_child(body, child, &cref);
		if (exc != DOM_NO_ERR) goto out;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(cref);

		dom_node *spare = NULL;
		exc = dom_node_insert_before(parent, child, el, &spare);

		if (exc != DOM_NO_ERR) goto out;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(spare);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(cref);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
		child = NULL;
		exc = dom_node_get_first_child(body, &child);
		if (exc != DOM_NO_ERR) goto out;
	}
	exc = dom_node_remove_child(parent, el, &cref);

	if (exc != DOM_NO_ERR) goto out;
out:
	if (parser != NULL) {
		dom_hubbub_parser_destroy(parser);
	}
	if (doc != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(doc);
	}
	if (fragment != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(fragment);
	}
	if (child != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(child);
	}
	if (html != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(html);
	}
	if (bodies != NULL) {
		dom_nodelist_unref(bodies);
	}
	if (body != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(body);
	}
	if (cref != NULL) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(cref);
	}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(parent);
	interpreter->changed = 1;

	js_pushundefined(J);
}
#endif

static void
mjs_fragment_set_property_textContent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	dom_string *content = NULL;
	dom_exception exc = dom_string_create((const uint8_t *)str, strlen(str), &content);

	if (exc != DOM_NO_ERR || !content) {
		js_error(J, "error");
		return;
	}
	exc = dom_node_set_text_content(el, content);
	dom_string_unref(content);

	js_pushundefined(J);
}

#if 0
static void
mjs_fragment_set_property_title(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_string *titlestr = NULL;
	dom_exception exc;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	assert(interpreter);
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	exc = dom_string_create((const uint8_t *)str, strlen(str), &titlestr);

	if (exc == DOM_NO_ERR && titlestr) {
		exc = dom_element_set_attribute(el, corestring_dom_title, titlestr);
		interpreter->changed = 1;
		dom_string_unref(titlestr);
	}
	js_pushundefined(J);
}
#endif

#if 0
static void
mjs_fragment_set_property_value(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct view_state *vs = interpreter->vs;
	struct document_view *doc_view;
	struct document *doc;
	struct el_form_control *fc;
	struct form_state *fs;
	struct link *link;
	int offset, linknum;

	if (!vs) {
		js_pushundefined(J);
		return;
	}
	doc_view = vs->doc_view;

	if (!doc_view) {
		js_pushundefined(J);
		return;
	}
	doc = doc_view->document;

	if (!el) {
		js_pushundefined(J);
		return;
	}
	offset = find_offset(doc->element_map_rev, el);

	if (offset < 0) {
		js_pushundefined(J);
		return;
	}
	linknum = get_link_number_by_offset(doc, offset);

	if (linknum < 0) {
		js_pushundefined(J);
		return;
	}

	link = &doc->links[linknum];
	fc = get_link_form_control(link);

	if (!fc) {
		js_pushundefined(J);
		return;
	}
	fs = find_form_state(doc_view, fc);

	if (!fs) {
		js_pushundefined(J);
		return;
	}

	if (fc->type != FC_FILE) {
		const char *str = js_tostring(J, 1);
		char *string;

		if (!str) {
			js_error(J, "!str");
			return;
		}

		string = stracpy(str);
		mem_free_set(&fs->value, string);

		if (fc->type == FC_TEXT || fc->type == FC_PASSWORD) {
			fs->state = strlen(fs->value);
		}
	}
	js_pushundefined(J);
}
#endif

#if 0
// Common part of all add_child_element*() methods.
static xmlpp::Element*
el_add_child_element_common(xmlNode* child, xmlNode* node)
{
	if (!node) {
		xmlFreeNode(child);
		throw xmlpp::internal_error("Could not add child element node");
	}
	xmlpp::Node::create_wrapper(node);

	return static_cast<xmlpp::Element*>(node->_private);
}
#endif

static void
mjs_fragment_addEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_fragment_private *el_private = (struct mjs_fragment_private *)js_touserdata(J, 0, "fragment");

	if (!el_private) {
		js_pushnull(J);
		return;
	}
	dom_node *el = el_private->node;

	if (!el) {
		js_pushnull(J);
		return;
	}

	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "out of memory");
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_error(J, "out of memory");
		return;
	}
	js_copy(J, 2);
	const char *fun = js_ref(J);
	struct fragment_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}

		if (l->fun == fun) {
			mem_free(method);
			js_pushundefined(J);
			return;
		}
	}
	struct fragment_listener *n = (struct fragment_listener *)mem_calloc(1, sizeof(*n));

	if (!n) {
		js_pushundefined(J);
		return;
	}
	n->fun = fun;
	n->typ = method;
	add_to_list_end(el_private->listeners, n);
	dom_exception exc;

	if (el_private->listener) {
		dom_event_listener_ref(el_private->listener);
	} else {
		exc = dom_event_listener_create(fragment_event_handler, el_private, &el_private->listener);

		if (exc != DOM_NO_ERR || !el_private->listener) {
			js_pushundefined(J);
			return;
		}
	}
	dom_string *typ = NULL;
	exc = dom_string_create((const uint8_t *)method, strlen(method), &typ);

	if (exc != DOM_NO_ERR || !typ) {
		goto ex;
	}
	exc = dom_event_target_add_event_listener(el, typ, el_private->listener, false);

	if (exc == DOM_NO_ERR) {
		dom_event_listener_ref(el_private->listener);
	}

ex:
	if (typ) {
		dom_string_unref(typ);
	}
	dom_event_listener_unref(el_private->listener);
	js_pushundefined(J);
}

static void
mjs_fragment_removeEventListener(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_fragment_private *el_private = (struct mjs_fragment_private *)js_touserdata(J, 0, "fragment");

	if (!el_private) {
		js_pushnull(J);
		return;
	}
	dom_node *el = el_private->node;

	if (!el) {
		js_pushnull(J);
		return;
	}

	const char *str = js_tostring(J, 1);

	if (!str) {
		js_error(J, "!str");
		return;
	}
	char *method = stracpy(str);

	if (!method) {
		js_error(J, "out of memory");
		return;
	}
	js_copy(J, 2);
	const char *fun = js_ref(J);
	struct fragment_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, method)) {
			continue;
		}
		if (l->fun == fun) {
			dom_string *typ = NULL;
			dom_exception exc = dom_string_create((const uint8_t *)method, strlen(method), &typ);

			if (exc != DOM_NO_ERR || !typ) {
				continue;
			}
			//dom_event_target_remove_event_listener(el, typ, el_private->listener, false);
			dom_string_unref(typ);

			js_unref(J, l->fun);
			del_from_list(l);
			mem_free_set(&l->typ, NULL);
			mem_free(l);
			mem_free(method);

			js_pushundefined(J);
			return;
		}
	}
	mem_free(method);
	js_pushundefined(J);
}

static void
mjs_fragment_appendChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *res = NULL;
	dom_exception exc;

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate_fragment(J, 1));
	exc = dom_node_append_child(el, el2, &res);

	if (exc == DOM_NO_ERR && res) {
		interpreter->changed = 1;
		mjs_push_element(J, res);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(res);
		return;
	}
	js_pushnull(J);
}

static void
mjs_fragment_cloneNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	dom_exception exc;
	bool deep = js_toboolean(J, 1);
	dom_node *clone = NULL;
	exc = dom_node_clone_node(el, deep, &clone);

	if (exc != DOM_NO_ERR || !clone) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, clone);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(clone);
}

#if 0
static bool
isAncestor(dom_node *el, dom_node *node)
{
	dom_node *prev_next = NULL;
	while (node) {
		dom_exception exc;
		dom_node *next = NULL;
		if (prev_next) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(prev_next);
		}
		if (el == node) {
			return true;
		}
		exc = dom_node_get_parent_node(node, &next);
		if (exc != DOM_NO_ERR || !next) {
			break;
		}
		prev_next = next;
		node = next;
	}

	return false;
}
#endif


static void
mjs_fragment_contains(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate_fragment(J, 1));

	if (!el2) {
		js_pushboolean(J, 0);
		return;
	}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_ref(el);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_ref(el2);

	while (1) {
		if (el == el2) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el2);
			js_pushboolean(J, 1);
			return;
		}
		dom_node *node = NULL;
		dom_exception exc = dom_node_get_parent_node(el2, &node);

		if (exc != DOM_NO_ERR || !node) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(el2);
			js_pushboolean(J, 0);
			return;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(el2);
		el2 = node;
	}
}


static void
mjs_fragment_hasChildNodes(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_exception exc;
	bool res;

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	exc = dom_node_has_child_nodes(el, &res);

	if (exc != DOM_NO_ERR) {
		js_pushboolean(J, 0);
		return;
	}
	js_pushboolean(J, res);
}

static void
mjs_fragment_insertBefore(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushundefined(J);
		return;
	}
	dom_node *next_sibling = (dom_node *)(mjs_getprivate_fragment(J, 2));

	if (!next_sibling) {
		js_pushnull(J);
		return;
	}

	dom_node *child = (dom_node *)(mjs_getprivate_fragment(J, 1));

	dom_exception err;
	dom_node *spare;

	err = dom_node_insert_before(el, child, next_sibling, &spare);
	if (err != DOM_NO_ERR) {
		js_pushundefined(J);
		return;
	}
	interpreter->changed = 1;
	mjs_push_element(J, spare);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(spare);
}

static void
mjs_fragment_isEqualNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate_fragment(J, 1));

	struct string first;
	struct string second;

	if (!init_string(&first)) {
		js_error(J, "out of memory");
		return;
	}
	if (!init_string(&second)) {
		done_string(&first);
		js_error(J, "out of memory");
		return;
	}
	ecmascript_walk_tree(&first, el, false, true);
	ecmascript_walk_tree(&second, el2, false, true);
	bool ret = !strcmp(first.source, second.source);
	done_string(&first);
	done_string(&second);
	js_pushboolean(J, ret);
}

static void
mjs_fragment_isSameNode(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_node *el2 = (dom_node *)(mjs_getprivate_fragment(J, 1));

	js_pushboolean(J, (el == el2));
}

static void
mjs_fragment_querySelector(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	const char *selector = js_tostring(J, 1);

	if (!selector) {
		js_pushnull(J);
		return;
	}
	void *ret = walk_tree_query(el, selector, 0);

	if (!ret) {
		js_pushnull(J);
		return;
	}
	mjs_push_element(J, ret);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(ret);
}

static void
mjs_fragment_querySelectorAll(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	struct document_view *doc_view = interpreter->vs->doc_view;
	struct document *document = doc_view->document;

	if (!document->dom) {
		js_pushnull(J);
		return;
	}
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushnull(J);
		return;
	}
	const char *selector = js_tostring(J, 1);

	if (!selector) {
		js_pushnull(J);
		return;
	}
	LIST_OF(struct selector_node) *result_list = (LIST_OF(struct selector_node) *)mem_calloc(1, sizeof(*result_list));

	if (!result_list) {
		js_pushnull(J);
		return;
	}
	init_list(*result_list);
	walk_tree_query_append(el, selector, 0, result_list);
	mjs_push_nodelist2(J, result_list);
}

static void
mjs_fragment_removeChild(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));
	dom_node *el2 = (dom_node *)(mjs_getprivate_fragment(J, 1));
	dom_exception exc;
	dom_node *spare;
	exc = dom_node_remove_child(el, el2, &spare);

	if (exc == DOM_NO_ERR && spare) {
		interpreter->changed = 1;
		mjs_push_element(J, spare);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(spare);
		return;
	}
	js_pushnull(J);
}

static void
mjs_fragment_toString(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	js_pushstring(J, "[DocumentFragment object]");
}

static void
mjs_fragment_finalizer(js_State *J, void *priv)
{
	struct mjs_fragment_private *el_private = (struct mjs_fragment_private *)priv;

	if (el_private) {
		dom_node *node = (dom_node *)(el_private->node);

		if (node && (node->refcnt > 0)) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(node);
		}
		mem_free(el_private);
	}
}

void
mjs_push_fragment(js_State *J, void *node)
{
	struct mjs_fragment_private *el_private = NULL;

#if 0
	void *second = attr_find_in_map(map_privates, node);

	if (second) {
		el_private = (struct mjs_fragment_private *)second;

		if (!attr_find_in_map(map_elements, el_private)) {
			el_private = NULL;
		} else {
			el_private->ref_count++;
		}
	}
#endif

	if (!el_private) {
		el_private = (struct mjs_fragment_private *)mem_calloc(1, sizeof(*el_private));

		if (!el_private) {
			js_pushnull(J);
			return;
		}
		el_private->node = node;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_ref((dom_node *)node);
	}

	js_newobject(J);
	{
		js_copy(J, 0);
		el_private->thisval = js_ref(J);
		js_newuserdata(J, "fragment", el_private, mjs_fragment_finalizer);
		addmethod(J, "addEventListener", mjs_fragment_addEventListener, 3);
		addmethod(J, "appendChild",mjs_fragment_appendChild, 1);
		addmethod(J, "cloneNode",	mjs_fragment_cloneNode, 1);
		addmethod(J, "contains",	mjs_fragment_contains, 1);
		addmethod(J, "dispatchEvent",	mjs_fragment_dispatchEvent, 1);
		addmethod(J, "hasChildNodes",	mjs_fragment_hasChildNodes, 0);
		addmethod(J, "insertBefore",	mjs_fragment_insertBefore, 2);
		addmethod(J, "isEqualNode",	mjs_fragment_isEqualNode, 1);
		addmethod(J, "isSameNode",		mjs_fragment_isSameNode, 1);
		addmethod(J, "querySelector",	mjs_fragment_querySelector, 1);
		addmethod(J, "querySelectorAll",	mjs_fragment_querySelectorAll, 1);
		addmethod(J, "removeChild",	mjs_fragment_removeChild, 1);
		addmethod(J, "removeEventListener", mjs_fragment_removeEventListener, 3);
		addmethod(J, "toString",		mjs_fragment_toString, 0);

////		addproperty(J, "attributes",	mjs_fragment_get_property_attributes, NULL);
////		addproperty(J, "checked",	mjs_fragment_get_property_checked, mjs_fragment_set_property_checked);
		addproperty(J, "children",	mjs_fragment_get_property_children, NULL);
		addproperty(J, "childElementCount",	mjs_fragment_get_property_childElementCount, NULL);
		addproperty(J, "childNodes",	mjs_fragment_get_property_childNodes, NULL);
////		addproperty(J, "classList",	mjs_fragment_get_property_classList, NULL);
////		addproperty(J, "className",	mjs_fragment_get_property_className, mjs_fragment_set_property_className);
//		addproperty(J, "clientHeight",	mjs_fragment_get_property_clientHeight, NULL);
//		addproperty(J, "clientLeft", mjs_fragment_get_property_clientLeft, NULL);
//		addproperty(J, "clientTop", mjs_fragment_get_property_clientTop, NULL);
//		addproperty(J, "clientWidth", mjs_fragment_get_property_clientWidth, NULL);
////		addproperty(J, "dataset",	mjs_fragment_get_property_dataset, NULL);
////		addproperty(J, "dir",	mjs_fragment_get_property_dir, mjs_fragment_set_property_dir);
		addproperty(J, "firstChild",	mjs_fragment_get_property_firstChild, NULL);
		addproperty(J, "firstElementChild",	mjs_fragment_get_property_firstElementChild, NULL);
////		addproperty(J, "href",	mjs_fragment_get_property_href, mjs_fragment_set_property_href);
		addproperty(J, "id",	mjs_fragment_get_property_id, mjs_fragment_set_property_id);
////		addproperty(J, "innerHTML",	mjs_fragment_get_property_innerHtml, mjs_fragment_set_property_innerHtml);
////		addproperty(J, "innerText",	mjs_fragment_get_property_innerHtml, mjs_fragment_set_property_innerText);
////		addproperty(J, "lang",	mjs_fragment_get_property_lang, mjs_fragment_set_property_lang);
		addproperty(J, "lastChild",	mjs_fragment_get_property_lastChild, NULL);
		addproperty(J, "lastElementChild",	mjs_fragment_get_property_lastElementChild, NULL);
////		addproperty(J, "nextElementSibling",	mjs_fragment_get_property_nextElementSibling, NULL);
////		addproperty(J, "nextSibling",	mjs_fragment_get_property_nextSibling, NULL);
		addproperty(J, "nodeName",	mjs_fragment_get_property_nodeName, NULL);
		addproperty(J, "nodeType",	mjs_fragment_get_property_nodeType, NULL);
		addproperty(J, "nodeValue",	mjs_fragment_get_property_nodeValue, NULL);
//		addproperty(J, "offsetHeight",	mjs_fragment_get_property_offsetHeight, NULL);
//		addproperty(J, "offsetLeft",	mjs_fragment_get_property_offsetLeft, NULL);
////		addproperty(J, "offsetParent",	mjs_fragment_get_property_offsetParent, NULL);
//		addproperty(J, "offsetTop",	mjs_fragment_get_property_offsetTop, NULL);
//		addproperty(J, "offsetWidth", mjs_fragment_get_property_offsetWidth, NULL);
////		addproperty(J, "outerHTML",	mjs_fragment_get_property_outerHtml, mjs_fragment_set_property_outerHtml);
		addproperty(J, "ownerDocument",	mjs_fragment_get_property_ownerDocument, NULL);
		addproperty(J, "parentElement",	mjs_fragment_get_property_parentElement, NULL);
		addproperty(J, "parentNode",	mjs_fragment_get_property_parentNode, NULL);
		addproperty(J, "previousElementSibling",	mjs_fragment_get_property_previousElementSibling, NULL);
		addproperty(J, "previousSibling",	mjs_fragment_get_property_previousSibling, NULL);
////		addproperty(J, "style",		mjs_fragment_get_property_style, NULL);
////		addproperty(J, "tagName",	mjs_fragment_get_property_tagName, NULL);
		addproperty(J, "textContent",	mjs_fragment_get_property_textContent, mjs_fragment_set_property_textContent);
////		addproperty(J, "title",	mjs_fragment_get_property_title, mjs_fragment_set_property_title);
////		addproperty(J, "value", mjs_fragment_get_property_value, mjs_fragment_set_property_value);
	}
}

int
mjs_fragment_init(js_State *J)
{
#if 0
	mjs_push_element(J, NULL);
	js_defglobal(J, "fragment", JS_DONTENUM);
#endif
	return 0;
}

#if 0
void
check_element_event(void *interp, void *elem, const char *event_name, struct term_event *ev)
{
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)interp;
	js_State *J = (js_State *)interpreter->backend_data;
	void *second = attr_find_in_map(map_privates, elem);

	if (!second) {
		return;
	}
	struct mjs_fragment_private *el_private = (struct mjs_fragment_private *)second;

	struct fragment_listener *l;

	foreach(l, el_private->listeners) {
		if (strcmp(l->typ, event_name)) {
			continue;
		}

		if (ev && ev->ev == EVENT_KBD && (!strcmp(event_name, "keydown") || !strcmp(event_name, "keyup") || !strcmp(event_name, "keypress"))) {
			js_getregistry(J, l->fun); /* retrieve the js function from the registry */
			js_getregistry(J, el_private->thisval);
			mjs_push_keyboardEvent(J, ev, event_name);
			js_pcall(J, 1);
			js_pop(J, 1);
		} else {
			js_getregistry(J, l->fun); /* retrieve the js function from the registry */
			js_getregistry(J, el_private->thisval);
			js_pushundefined(J);
			js_pcall(J, 1);
			js_pop(J, 1);
		}
	}
	check_for_rerender(interpreter, event_name);
}
#endif

static void
mjs_fragment_dispatchEvent(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(mjs_getprivate_fragment(J, 0));

	if (!el) {
		js_pushboolean(J, 0);
		return;
	}
	dom_event *event = (dom_event *)js_touserdata(J, 1, "event");

	if (!event) {
		js_pushboolean(J, 0);
		return;
	}
	bool result = false;
	(void)dom_event_target_dispatch_event(el, event, &result);
	js_pushboolean(J, result);
}

static void
fragment_event_handler(dom_event *event, void *pw)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct mjs_fragment_private *el_private = (struct mjs_fragment_private *)pw;
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)el_private->interpreter;
	js_State *J = (js_State *)interpreter->backend_data;

	if (!event) {
		return;
	}

	dom_string *typ = NULL;
	dom_exception exc = dom_event_get_type(event, &typ);

	if (exc != DOM_NO_ERR || !typ) {
		return;
	}
//	interpreter->heartbeat = add_heartbeat(interpreter);

	struct fragment_listener *l, *next;

	foreachsafe(l, next, el_private->listeners) {
		if (strcmp(l->typ, dom_string_data(typ))) {
			continue;
		}
		js_getregistry(J, l->fun); /* retrieve the js function from the registry */
		js_getregistry(J, el_private->thisval);
		mjs_push_event(J, event);
		js_pcall(J, 1);
		js_pop(J, 1);
	}
//	done_heartbeat(interpreter->heartbeat);
	check_for_rerender(interpreter, dom_string_data(typ));
	dom_string_unref(typ);
}