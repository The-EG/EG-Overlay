/*** RST
libxml2
=======

.. |parse error returns| replace::
        Parsing errors are printed to the log. In some cases parsing will
        continue, but in others parsing will fail in which case this function.
        returns ``nil``.

.. lua:module:: libxml2

.. code-block:: lua

    local xml = require 'libxml2'

The :lua:mod:`libxml2` module is, as its name suggests, a Lua binding to
`libxml2 <https://gitlab.gnome.org/GNOME/libxml2>`_.

*/
#include "xml.h"
#include "utils.h"
#include "logging/logger.h"
#include "lua-manager.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <lauxlib.h>
#include <string.h>

int xml_lua_open_module(lua_State *L);

void xml_lua_init() {
    lua_manager_add_module_opener("libxml2", &xml_lua_open_module);
}

void xml_cleanup() {
    xmlCleanupParser();
}

int xml_lua_read_file(lua_State *L);
int xml_lua_read_string(lua_State *L);

void xml_doc_lua_register_metatable(lua_State *L);
void lua_pushxmldoc(lua_State *L, xmlDocPtr doc, int lua_managed);
xmlDocPtr lua_checkxmldoc(lua_State *L, int ind);

void xml_node_lua_register_metatable(lua_State *L);
void lua_pushxmlnode(lua_State *L, xmlNodePtr node, int lua_managed);
xmlNodePtr lua_checkxmlnode(lua_State *L, int ind);

static luaL_Reg xml_funcs[] = {
    "read_file",   &xml_lua_read_file,
    "read_string", &xml_lua_read_string,
    NULL, NULL
};

int xml_lua_open_module(lua_State *L) {
    lua_newtable(L);
    luaL_setfuncs(L, xml_funcs, 0);
    
    return 1;
}

typedef struct error_handler_data_t {
    char *file;
    size_t file_size;
} error_handler_data_t;

void xml_error_handler(error_handler_data_t *data, const xmlError *error) {
    logger_t *log = logger_get("xml");
    enum LOGGER_LEVEL level = LOGGER_LEVEL_DEBUG;
    switch (error->level) {
    /* XML_ERR_ERROR is still recoverable, where FATAL is not */
    case XML_ERR_WARNING:
    case XML_ERR_ERROR  : level = LOGGER_LEVEL_WARNING; break;
    case XML_ERR_FATAL  : level = LOGGER_LEVEL_ERROR  ; break;
    }

    size_t line_start = 0;
    size_t line_len = 0;
    int line_no = 1;

    for (size_t i=0;i<data->file_size; i++) {
        if (data->file[i]=='\n') {
            line_len = i - line_start;
            if (line_no==error->line) break;
            line_no++;
            line_start = i + 1;
        }
    }

    char *msg = egoverlay_calloc(strlen(error->message), sizeof(char));
    memcpy(msg, error->message, strlen(error->message)-1); // strip the trailing \n
    logger_log(log, level, "XML parsing error: %s:%d:%d : %s", error->file, error->line, error->int2, msg);
    egoverlay_free(msg);
    /*
    if (line_len) {
        size_t context_begin = line_start + error->int2 - 20;
        if (context_begin < line_start) context_begin = line_start;

        size_t context_len = 41;
        if (context_begin - line_start + context_len > line_len) {
            context_len = line_len - (context_begin - line_start);
        }

        char *context = egoverlay_calloc(context_len + 1, sizeof(char));
        memcpy(context, data->file + context_begin, context_len);

        logger_log(log, level, "  %s", context);
        egoverlay_free(context);

        logger_log(log, level, "  % *s", error->int2 - (context_begin - line_start), "^");
    }
    */
}

/*** RST
Functions
---------

.. lua:function:: read_file(path)

    Read xml from ``path`` and parse into an XML document.

    .. important:: |parse error returns|

    :param string path:
    :rtype: XMLDoc

    .. versionhistory::
        :0.0.1: Added
*/
int xml_lua_read_file(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);

    xmlDocPtr doc = xmlReadFile(path, NULL, 0);
    if (doc) lua_pushxmldoc(L, doc, 1);
    else lua_pushnil(L);

    return 1;
}

typedef struct {
    lua_State *L;
    xmlParserCtxtPtr ctx;
    int startelement;
    int endelement;
} xml_lua_sax_cbs_t;

void xml_lua_sax_start_element_ns(
    xml_lua_sax_cbs_t *cbs,
    const xmlChar *localname,
    const xmlChar *prefix,
    const xmlChar *URI,
    int nb_namespaces,
    const xmlChar **namespaces,
    int nb_attributes,
    int nb_defaulted,
    const xmlChar **attributes
) {
    UNUSED_PARAM(prefix);
    UNUSED_PARAM(URI);
    UNUSED_PARAM(nb_namespaces);
    UNUSED_PARAM(namespaces);
    UNUSED_PARAM(nb_defaulted);


    if (cbs->startelement==0) return;

    lua_rawgeti(cbs->L, LUA_REGISTRYINDEX, cbs->startelement);

    lua_pushstring(cbs->L, (const char*)localname);

    lua_createtable(cbs->L, 0, nb_attributes);
    for (int a=0;a<nb_attributes;a++) {
        const char *attrname = (const char *)attributes[a * 5];
        const char *attrvalue = (const char *)attributes[(a * 5) + 3];
        const char *attrvalend = (const char *)attributes[(a * 5) + 4];
        size_t attrvallen = (size_t)(attrvalend) - (size_t)(attrvalue);
        lua_pushlstring(cbs->L, attrvalue, attrvallen);
        lua_setfield(cbs->L, -2, attrname);
    }

    lua_pushstring(cbs->L, cbs->ctx->input->filename);
    lua_pushinteger(cbs->L, cbs->ctx->input->line);

    int r = lua_pcall(cbs->L, 4, 0, 0);

    if (r!=LUA_OK) {
        logger_t *log = logger_get("xml");
        const char *msg = lua_tostring(cbs->L, -1);
        logger_error(log, "Error during startelement callback: %s", msg);
        lua_pop(cbs->L, 1);
    }
}

void xml_lua_sax_end_element_ns(
    xml_lua_sax_cbs_t *cbs,
    const xmlChar *localname,
    const xmlChar *prefix,
    const xmlChar *URI
) {
    UNUSED_PARAM(prefix);
    UNUSED_PARAM(URI);

    if (cbs->endelement==0) return;

    lua_rawgeti(cbs->L, LUA_REGISTRYINDEX, cbs->endelement);

    lua_pushstring(cbs->L, (const char *)localname);
    lua_pushstring(cbs->L, cbs->ctx->input->filename);
    lua_pushinteger(cbs->L, cbs->ctx->input->line);

    int r = lua_pcall(cbs->L, 3, 0, 0);

    if (r!=LUA_OK) {
        logger_t *log = logger_get("xml");
        const char *msg = lua_tostring(cbs->L, -1);
        logger_error(log, "Error during endelement callback: %s", msg);
        lua_pop(cbs->L, 1);
    }
}

/*** RST
.. lua:function:: read_string(xml, name[, callbacks])

    Parse xml from ``xml``. If ``callbacks`` are supplied SAX based parsing can
    be performed. The following fields will be called if they exist on
    ``callbacks``:

    +----------------+---------------------------------------------------------+
    | Field/Function | Description                                             |
    +================+=========================================================+
    | startelement   | Called at an opening element with the element name, a   |
    |                | table of attributes, the name of the current document,  |
    |                | and the current line this element occurs on.            |
    |                |                                                         |
    |                | .. code-block:: lua                                     |
    |                |                                                         |
    |                |     local function startelement(name, attrs, doc, line) |
    |                |                                                         |
    |                |     end                                                 |
    +----------------+---------------------------------------------------------+
    | endelement     | Called at a closing element with the element name, the  |
    |                | name of the current document, and the current line.     |
    |                |                                                         |
    |                | .. code-block:: lua                                     |
    |                |                                                         |
    |                |     local function endelement(name, doc, line)          |
    |                |                                                         |
    |                |     end                                                 |
    +----------------+---------------------------------------------------------+

    .. important:: |parse error returns|

    :param string xml: The XML string to parse.
    :param string name: The document name, typically a path or file name.
    :param table callbacks: (Optional)
    :rtype: XMLDoc

    .. versionhistory::
        :0.0.1: Added
        :0.1.0: Added callbacks parameter, SAX2 parsing
*/
int xml_lua_read_string(lua_State *L) {
    int data_size = 0;
    const char *data = luaL_checklstring(L, 1, (size_t*)&data_size);
    const char *name = luaL_checkstring(L, 2);

    error_handler_data_t err_data = {0};
    err_data.file = (char *)data; // data will remain in scope while err_data does, so reference directly
    err_data.file_size = data_size;

    xmlSAXHandler sh = {0};
    sh.initialized = XML_SAX2_MAGIC;
    sh.startElementNs = &xml_lua_sax_start_element_ns;
    sh.endElementNs = &xml_lua_sax_end_element_ns;

    xmlParserCtxtPtr ctx = NULL;

    xml_lua_sax_cbs_t *cbs = NULL;
    if (lua_gettop(L)==3 && lua_type(L,3)==LUA_TTABLE) {
        cbs = egoverlay_calloc(1, sizeof(xml_lua_sax_cbs_t));

        cbs->L = L;

        if (lua_getfield(L, 3, "startelement")==LUA_TFUNCTION) {
            cbs->startelement = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            lua_pop(L, 1);
        }

        if (lua_getfield(L, 3, "endelement")==LUA_TFUNCTION) {
            cbs->endelement = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            lua_pop(L, 1);
        }

        ctx = xmlNewSAXParserCtxt(&sh, cbs);
        cbs->ctx = ctx;
    } else {
        ctx = xmlNewParserCtxt();
    }

    xmlCtxtSetOptions(ctx, XML_PARSE_NOBLANKS | XML_PARSE_RECOVER | XML_PARSE_NOENT);
    xmlCtxtSetErrorHandler(ctx, &xml_error_handler, &err_data);

    xmlDocPtr doc = xmlCtxtReadMemory(ctx, data, data_size, name, NULL, XML_PARSE_RECOVER | XML_PARSE_NOENT);

    xmlFreeParserCtxt(ctx);
    if (doc) lua_pushxmldoc(L, doc, 1);
    else lua_pushnil(L);

    if (cbs) {
        if (cbs->startelement) luaL_unref(L, LUA_REGISTRYINDEX, cbs->startelement);
        if (cbs->endelement) luaL_unref(L, LUA_REGISTRYINDEX, cbs->endelement);
        egoverlay_free(cbs);
    }

    return 1;
}

/*** RST
Classes
-------

.. lua:class:: XMLDoc

    An XML Document.

*/
void lua_pushxmldoc(lua_State *L, xmlDocPtr doc, int lua_managed) {
    xmlDocPtr *ppdoc = (xmlDocPtr*)lua_newuserdata(L, sizeof(xmlDocPtr));
    *ppdoc = doc;

    //logger_t *log = logger_get("xml");
    // if (lua_managed) logger_debug(log, "Pushing XMLDoc 0x%x (%s) to Lua", doc, doc->URL);

    lua_pushboolean(L, lua_managed);
    lua_setiuservalue(L, -2, 1);
    xml_doc_lua_register_metatable(L);
    lua_setmetatable(L, -2);
}

int xml_doc_lua_del(lua_State *L);
int xml_doc_lua_get_root_element(lua_State *L);
int xml_doc_lua_name(lua_State *L);
int xml_doc_lua_url(lua_State *L);

luaL_Reg xml_doc_funcs[] = {
    "__gc",             &xml_doc_lua_del,
    "get_root_element", &xml_doc_lua_get_root_element,
    "name",             &xml_doc_lua_name,
    "url",              &xml_doc_lua_url,
    NULL,          NULL
};

void xml_doc_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "XmlDocMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, xml_doc_funcs, 0);
    }
}

xmlDocPtr lua_checkxmldoc(lua_State *L, int ind) {
    return *(xmlDocPtr*)luaL_checkudata(L, ind, "XmlDocMetaTable");
}

int xml_doc_lua_del(lua_State *L) {
    xmlDocPtr doc = lua_checkxmldoc(L, 1);

    lua_getiuservalue(L, -1, 1);
    int lua_managed = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if (lua_managed) {
        // logger_t *log = logger_get("xml");
        // logger_debug(log, "Freeing XMLDoc 0x%x (%s)", doc, doc->URL);
        xmlFreeDoc(doc);
    }
    return 0;
}

/*** RST
    .. lua:method:: get_root_element()

        Return the root element for this document.

        :rtype: XMLNode

        .. versionhistory::
            :0.0.0: Added
*/
int xml_doc_lua_get_root_element(lua_State *L) {
    xmlDocPtr doc = lua_checkxmldoc(L, 1);

    xmlNodePtr node = xmlDocGetRootElement(doc);

    lua_pushxmlnode(L, node, 0);

    return 1;
}

/*** RST
    .. lua:method:: name()

        Return the name of this document. This is the name supplied with
        :lua:func:`read_string`.

        :rtype: string

        .. versionhistory::
            :0.0.1: Added
*/
int xml_doc_lua_name(lua_State *L) {
    xmlDocPtr doc = lua_checkxmldoc(L, 1);

    lua_pushstring(L, doc->name);

    return 1;
}

/*** RST
    .. lua:method:: url()

        Return the URL of this document, or the :lua:meth:`name` if it doesn't
        have one.

        :rtype: string

        .. versionhistory::
            :0.0.1: Added
*/
int xml_doc_lua_url(lua_State *L) {
    xmlDocPtr doc = lua_checkxmldoc(L, 1);

    lua_pushstring(L, (const char*)doc->URL);

    return 1;
}


/*** RST
.. lua:class:: XMLNode

    A node within a :lua:class:`XMLDoc`. This could be an element with children,
    a text node, whitespace or comment.
*/

int xml_node_lua_del(lua_State *L);
int xml_node_lua_copy(lua_State *L);
int xml_node_lua_prev(lua_State *L);
int xml_node_lua_next(lua_State *L);
int xml_node_lua_children(lua_State *L);
int xml_node_lua_type(lua_State *L);
int xml_node_lua_name(lua_State *L);
int xml_node_lua_prop(lua_State *L);
int xml_node_lua_props(lua_State *L);
int xml_node_lua_content(lua_State *L);
int xml_node_lua_doc(lua_State *L);
int xml_node_lua_line(lua_State *L);

luaL_Reg xml_node_funcs[] = {
    "__gc"    , &xml_node_lua_del,
    "copy"    , &xml_node_lua_copy,
    "prev"    , &xml_node_lua_prev,
    "next"    , &xml_node_lua_next,
    "children", &xml_node_lua_children,
    "type"    , &xml_node_lua_type,
    "name"    , &xml_node_lua_name,
    "prop"    , &xml_node_lua_prop,
    "props"   , &xml_node_lua_props,
    "content" , &xml_node_lua_content,
    "doc"     , &xml_node_lua_doc,
    "line"    , &xml_node_lua_line,
    NULL      ,  NULL
};


void xml_node_lua_register_metatable(lua_State *L) {
    if (luaL_newmetatable(L, "XmlNodeMetaTable")) {
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");

        luaL_setfuncs(L, xml_node_funcs, 0);
    }
}

void lua_pushxmlnode(lua_State *L, xmlNodePtr node, int lua_managed) {
    xmlNodePtr *ppnode = (xmlNodePtr*)lua_newuserdata(L, sizeof(xmlNodePtr));
    *ppnode = node;

    lua_pushboolean(L, lua_managed);
    lua_setiuservalue(L, -2, 1);
    xml_node_lua_register_metatable(L);
    lua_setmetatable(L, -2);
}

xmlNodePtr lua_checkxmlnode(lua_State *L, int ind) {
    return *(xmlNodePtr*)luaL_checkudata(L, ind, "XmlNodeMetaTable");
}

int xml_node_lua_del(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    lua_getiuservalue(L, -1, 1);
    int lua_managed = lua_toboolean(L, -1);
    lua_pop(L, 1);

    if (lua_managed) {
        logger_t *log = logger_get("xml");
        logger_debug(log, "freeing xml node");
        xmlFreeNode(node);
    }
    return 0;
}

/*** RST
    .. lua:method:: copy()

        Clone this node.

        :rtype: XMLNode

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_copy(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    xmlNodePtr copy = xmlCopyNode(node, 1);

    lua_pushxmlnode(L, copy, 1);

    return 1;
}

/*** RST
    .. lua:method:: prev()

        Return the previous sibling of this node. In other words, the node that
        appears before this one in its parent's children.

        This function will return ``nil`` if there is no previous sibling.

        :rtype: XMLNode

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_prev(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    if (node->prev) lua_pushxmlnode(L, node->prev, 0);
    else lua_pushnil(L);

    return 1;
}

/*** RST
    .. lua:method:: next()

        Return the next sibling of this node. In other words, the node that
        appears after this one in its parent's children.

        This function will return ``nil`` if there is no next sibling.

        :rtype: XMLNode

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_next(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    if (node->next) lua_pushxmlnode(L, node->next, 0);
    else lua_pushnil(L);

    return 1;
}

/*** RST
    .. lua:method:: children()

        Return the *first* child of this node. The :lua:meth:`prev` and
        :lua:meth:`next` methods of that node can be used to traverse all child
        nodes.

        This function will return ``nil`` if this node has no children.

        :rtype: XMLNode

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_children(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    if (node->children) lua_pushxmlnode(L, node->children, 0);
    else lua_pushnil(L);

    return 1;
}

/*** RST
    .. lua:method:: type()

        Return a string indicating the type of this node. One of:

        * element-node
        * attribute-node
        * text-node
        * cdata-section-node
        * entity-ref-node
        * pi-node
        * comment-node
        * document-node
        * document-type-node
        * document-fragment-node
        * notation-node
        * html-document-node
        * dtd-node
        * element-declaration
        * attribute-declaration
        * entity-declaration
        * namespace-declaration
        * xinclude-start
        * xinclude-end
        * unknown
    
        :rtype: string

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_type(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    switch (node->type) {
    case XML_ELEMENT_NODE:       lua_pushliteral(L, "element-node");           break;
    case XML_ATTRIBUTE_NODE:     lua_pushliteral(L, "attribute-node");         break;
    case XML_TEXT_NODE:          lua_pushliteral(L, "text-node");              break;
    case XML_CDATA_SECTION_NODE: lua_pushliteral(L, "cdata-section-node");     break;
    case XML_ENTITY_REF_NODE:    lua_pushliteral(L, "entity-ref-node");        break;
    case XML_ENTITY_NODE:        lua_pushliteral(L, "entity-node");            break;
    case XML_PI_NODE:            lua_pushliteral(L, "pi-node");                break;
    case XML_COMMENT_NODE:       lua_pushliteral(L, "comment-node");           break;
    case XML_DOCUMENT_NODE:      lua_pushliteral(L, "document-node");          break;
    case XML_DOCUMENT_TYPE_NODE: lua_pushliteral(L, "document-type-node");     break;
    case XML_DOCUMENT_FRAG_NODE: lua_pushliteral(L, "document-fragment-node"); break;
    case XML_NOTATION_NODE:      lua_pushliteral(L, "notation-node");          break;
    case XML_HTML_DOCUMENT_NODE: lua_pushliteral(L, "html-document-node");     break;
    case XML_DTD_NODE:           lua_pushliteral(L, "dtd-node");               break;
    case XML_ELEMENT_DECL:       lua_pushliteral(L, "element-declaration");    break;
    case XML_ATTRIBUTE_DECL:     lua_pushliteral(L, "attribute-declaration");  break;
    case XML_ENTITY_DECL:        lua_pushliteral(L, "entity-declaration");     break;
    case XML_NAMESPACE_DECL:     lua_pushliteral(L, "namespace-declaration");  break;
    case XML_XINCLUDE_START:     lua_pushliteral(L, "xinclude-start");         break;
    case XML_XINCLUDE_END:       lua_pushliteral(L, "xinclude-end");           break;
    default:                     lua_pushliteral(L, "unknown");                break;
    }

    return 1;
}

/*** RST
    .. lua:method:: name()
    
        Return the name of this node. For element nodes, this will be the
        tag name. ``nil`` is returned if the node does not have a name.

        :rtype: string

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_name(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    if (node->name) lua_pushstring(L, (const char*)node->name);
    else lua_pushnil(L);

    return 1;
}

/*** RST
    .. lua:method:: prop(name[, value])

        Return or set the value of a property (attribute) on this node.

        If ``value`` is omitted, the value of ``name`` property is returned.

        If ``value`` is omitted and ``name`` does not exist on this node,
        ``nil`` is returned.

        :param string name: The property (attribute) name.
        :param string value: (Optional) If present, the property ``name`` is set
            to this value. If omitted, the value of ``name`` is returned.
        :rtype: string

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_prop(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);
    const char *prop_name = luaL_checkstring(L, 2);

    if (lua_gettop(L)==2) {
        char *prop_val = (char*)xmlGetProp(node, (const xmlChar*)prop_name);

        if (prop_val) {
            lua_pushstring(L, prop_val);
            xmlFree(prop_val);
        } else {
            lua_pushnil(L);
        }

        return 1;
    } else if (lua_gettop(L)==3) {
        const char *new_val = luaL_checkstring(L, 3);
        xmlSetProp(node, (const xmlChar*)prop_name, (const xmlChar*)new_val);
        return 0;
    }
    return luaL_error(L, "node:prop either takes one argument (returns property) or two arguments (sets value).");
}

/*** RST
    .. lua:method:: props()

        Return a sequence of property names on this node.

        :rtype: table

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_props(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    lua_newtable(L);
    int ti = 1;
    for (xmlAttrPtr p = node->properties; p ; p = p->next) {
        lua_pushstring(L, (const char*)p->name);
        lua_seti(L, -2, ti++);
    }

    return 1;
}

/*** RST
    .. lua:method:: content([text])

        Return or set the content of this node. If ``text`` is omitted, the
        content is returned, otherwise the content is set to ``text``

        :rtype: string

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_content(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    if (lua_gettop(L)==1) {
        char *content = (char*)xmlNodeGetContent(node);

        lua_pushstring(L, content);
        xmlFree(content);

        return 1;
    } else if (lua_gettop(L)==2) {
        const char *new_content = luaL_checkstring(L, 2);
        xmlNodeSetContent(node, (const xmlChar*)new_content);
        return 0;
    }
    return luaL_error(L, "node:content either takes no argument (returns content) "
                         "or 1 string argument (sets content).");
}

/*** RST
    .. lua:method:: doc()
    
        Return the :lua:class:`XMLDoc` this node belongs to.

        :rtype: XMLDoc

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_doc(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    if (node->doc) lua_pushxmldoc(L, node->doc, 0);
    else lua_pushnil(L);

    return 1;
}

/*** RST
    .. lua:method:: line()

        Return the line number this node occurs on within the input text.

        This can be used to provide informative error messages about malformed
        data.

        :rtype: integer

        .. versionhistory::
            :0.0.1: Added
*/
int xml_node_lua_line(lua_State *L) {
    xmlNodePtr node = lua_checkxmlnode(L, 1);

    lua_pushinteger(L, node->line);

    return 1;
}
