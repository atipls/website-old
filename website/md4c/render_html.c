/*
 * MD4C: Markdown parser for C
 * (http://github.com/mity/md4c)
 *
 * Copyright (c) 2016-2019 Martin Mitas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>

#include "render_html.h"
#include "entity.h"


#ifdef _MSC_VER
    /* MSVC does not understand "inline" when building as pure C (not C++).
     * However it understands "__inline" */
    #ifndef __cplusplus
        #define inline __inline
    #endif
#endif

#ifdef _WIN32
    #define snprintf _snprintf
#endif



typedef struct MD_RENDER_HTML_tag MD_RENDER_HTML;
struct MD_RENDER_HTML_tag {
    void (*process_output)(const MD_CHAR*, MD_SIZE, void*);
    void* userdata;
    unsigned flags;
    int image_nesting_level;
    char escape_map[256];
};


/*****************************************
 ***  HTML rendering helper functions  ***
 *****************************************/

#define ISDIGIT(ch)     ('0' <= (ch) && (ch) <= '9')
#define ISLOWER(ch)     ('a' <= (ch) && (ch) <= 'z')
#define ISUPPER(ch)     ('A' <= (ch) && (ch) <= 'Z')
#define ISALNUM(ch)     (ISLOWER(ch) || ISUPPER(ch) || ISDIGIT(ch))


static inline void
render_text(MD_RENDER_HTML* r, const MD_CHAR* text, MD_SIZE size)
{
    r->process_output(text, size, r->userdata);
}

#define RENDER_LITERAL(r, literal)    render_text((r), (literal), (MD_SIZE) strlen(literal))


static void
render_html_escaped(MD_RENDER_HTML* r, const MD_CHAR* data, MD_SIZE size)
{
    MD_OFFSET beg = 0;
    MD_OFFSET off = 0;

    /* Some characters need to be escaped in normal HTML text. */
    #define HTML_NEED_ESCAPE(ch)        (r->escape_map[(unsigned char)(ch)] != 0)

    while(1) {
        /* Optimization: Use some loop unrolling. */
        while(off + 3 < size  &&  !HTML_NEED_ESCAPE(data[off+0])  &&  !HTML_NEED_ESCAPE(data[off+1])
                              &&  !HTML_NEED_ESCAPE(data[off+2])  &&  !HTML_NEED_ESCAPE(data[off+3]))
            off += 4;
        while(off < size  &&  !HTML_NEED_ESCAPE(data[off]))
            off++;

        if(off > beg)
            render_text(r, data + beg, off - beg);

        if(off < size) {
            switch(data[off]) {
                case '&':   RENDER_LITERAL(r, "&amp;"); break;
                case '<':   RENDER_LITERAL(r, "&lt;"); break;
                case '>':   RENDER_LITERAL(r, "&gt;"); break;
                case '"':   RENDER_LITERAL(r, "&quot;"); break;
            }
            off++;
        } else {
            break;
        }
        beg = off;
    }
}

static void
render_url_escaped(MD_RENDER_HTML* r, const MD_CHAR* data, MD_SIZE size)
{
    static const MD_CHAR hex_chars[] = "0123456789ABCDEF";
    MD_OFFSET beg = 0;
    MD_OFFSET off = 0;

    #define URL_NEED_ESCAPE(ch)                                             \
            (!ISALNUM(ch)  &&  strchr("-_.+!*'(),%#@?=;:/,+$", ch) == NULL)

    while(1) {
        while(off < size  &&  !URL_NEED_ESCAPE(data[off]))
            off++;
        if(off > beg)
            render_text(r, data + beg, off - beg);

        if(off < size) {
            char hex[3];

            switch(data[off]) {
                case '&':   RENDER_LITERAL(r, "&amp;"); break;
                case '\'':  RENDER_LITERAL(r, "&#x27;"); break;
                default:
                    hex[0] = '%';
                    hex[1] = hex_chars[((unsigned)data[off] >> 4) & 0xf];
                    hex[2] = hex_chars[((unsigned)data[off] >> 0) & 0xf];
                    render_text(r, hex, 3);
                    break;
            }
            off++;
        } else {
            break;
        }

        beg = off;
    }
}

static unsigned
hex_val(char ch)
{
    if('0' <= ch && ch <= '9')
        return ch - '0';
    if('A' <= ch && ch <= 'Z')
        return ch - 'A' + 10;
    else
        return ch - 'a' + 10;
}

static void
render_utf8_codepoint(MD_RENDER_HTML* r, unsigned codepoint,
                      void (*fn_append)(MD_RENDER_HTML*, const MD_CHAR*, MD_SIZE))
{
    static const MD_CHAR utf8_replacement_char[] = { 0xef, 0xbf, 0xbd };

    unsigned char utf8[4];
    size_t n;

    if(codepoint <= 0x7f) {
        n = 1;
        utf8[0] = codepoint;
    } else if(codepoint <= 0x7ff) {
        n = 2;
        utf8[0] = 0xc0 | ((codepoint >>  6) & 0x1f);
        utf8[1] = 0x80 + ((codepoint >>  0) & 0x3f);
    } else if(codepoint <= 0xffff) {
        n = 3;
        utf8[0] = 0xe0 | ((codepoint >> 12) & 0xf);
        utf8[1] = 0x80 + ((codepoint >>  6) & 0x3f);
        utf8[2] = 0x80 + ((codepoint >>  0) & 0x3f);
    } else {
        n = 4;
        utf8[0] = 0xf0 | ((codepoint >> 18) & 0x7);
        utf8[1] = 0x80 + ((codepoint >> 12) & 0x3f);
        utf8[2] = 0x80 + ((codepoint >>  6) & 0x3f);
        utf8[3] = 0x80 + ((codepoint >>  0) & 0x3f);
    }

    if(0 < codepoint  &&  codepoint <= 0x10ffff)
        fn_append(r, (char*)utf8, n);
    else
        fn_append(r, utf8_replacement_char, 3);
}

/* Translate entity to its UTF-8 equivalent, or output the verbatim one
 * if such entity is unknown (or if the translation is disabled). */
static void
render_entity(MD_RENDER_HTML* r, const MD_CHAR* text, MD_SIZE size,
              void (*fn_append)(MD_RENDER_HTML*, const MD_CHAR*, MD_SIZE))
{
    if(r->flags & MD_RENDER_FLAG_VERBATIM_ENTITIES) {
        fn_append(r, text, size);
        return;
    }

    /* We assume UTF-8 output is what is desired. */
    if(size > 3 && text[1] == '#') {
        unsigned codepoint = 0;

        if(text[2] == 'x' || text[2] == 'X') {
            /* Hexadecimal entity (e.g. "&#x1234abcd;")). */
            MD_SIZE i;
            for(i = 3; i < size-1; i++)
                codepoint = 16 * codepoint + hex_val(text[i]);
        } else {
            /* Decimal entity (e.g. "&1234;") */
            MD_SIZE i;
            for(i = 2; i < size-1; i++)
                codepoint = 10 * codepoint + (text[i] - '0');
        }

        render_utf8_codepoint(r, codepoint, fn_append);
        return;
    } else {
        /* Named entity (e.g. "&nbsp;"). */
        const struct entity* ent;

        ent = entity_lookup(text, size);
        if(ent != NULL) {
            render_utf8_codepoint(r, ent->codepoints[0], fn_append);
            if(ent->codepoints[1])
                render_utf8_codepoint(r, ent->codepoints[1], fn_append);
            return;
        }
    }

    fn_append(r, text, size);
}

static void
render_attribute(MD_RENDER_HTML* r, const MD_ATTRIBUTE* attr,
                 void (*fn_append)(MD_RENDER_HTML*, const MD_CHAR*, MD_SIZE))
{
    int i;

    for(i = 0; attr->substr_offsets[i] < attr->size; i++) {
        MD_TEXTTYPE type = attr->substr_types[i];
        MD_OFFSET off = attr->substr_offsets[i];
        MD_SIZE size = attr->substr_offsets[i+1] - off;
        const MD_CHAR* text = attr->text + off;

        switch(type) {
            case MD_TEXT_NULLCHAR:  render_utf8_codepoint(r, 0x0000, render_text); break;
            case MD_TEXT_ENTITY:    render_entity(r, text, size, fn_append); break;
            default:                fn_append(r, text, size); break;
        }
    }
}


static void
render_open_ol_block(MD_RENDER_HTML* r, const MD_BLOCK_OL_DETAIL* det)
{
    char buf[64];

    if(det->start == 1) {
        RENDER_LITERAL(r, "<ol>\n");
        return;
    }

    snprintf(buf, sizeof(buf), "<ol start=\"%u\">\n", det->start);
    RENDER_LITERAL(r, buf);
}

static void
render_open_li_block(MD_RENDER_HTML* r, const MD_BLOCK_LI_DETAIL* det)
{
    if(det->is_task) {
        RENDER_LITERAL(r, "<li class=\"task-list-item\">"
                          "<input type=\"checkbox\" class=\"task-list-item-checkbox\" disabled");
        if(det->task_mark == 'x' || det->task_mark == 'X')
            RENDER_LITERAL(r, " checked");
        RENDER_LITERAL(r, ">");
    } else {
        RENDER_LITERAL(r, "<li>");
    }
}

static void
render_open_code_block(MD_RENDER_HTML* r, const MD_BLOCK_CODE_DETAIL* det)
{
    RENDER_LITERAL(r, "<pre><code");

    /* If known, output the HTML 5 attribute class="language-LANGNAME". */
    if(det->lang.text != NULL) {
        RENDER_LITERAL(r, " class=\"language-");
        render_attribute(r, &det->lang, render_html_escaped);
        RENDER_LITERAL(r, "\"");
    }

    RENDER_LITERAL(r, ">");
}

static void
render_open_td_block(MD_RENDER_HTML* r, const MD_CHAR* cell_type, const MD_BLOCK_TD_DETAIL* det)
{
    RENDER_LITERAL(r, "<");
    RENDER_LITERAL(r, cell_type);

    switch(det->align) {
        case MD_ALIGN_LEFT:     RENDER_LITERAL(r, " align=\"left\">"); break;
        case MD_ALIGN_CENTER:   RENDER_LITERAL(r, " align=\"center\">"); break;
        case MD_ALIGN_RIGHT:    RENDER_LITERAL(r, " align=\"right\">"); break;
        default:                RENDER_LITERAL(r, ">"); break;
    }
}

static void
render_open_a_span(MD_RENDER_HTML* r, const MD_SPAN_A_DETAIL* det)
{
    RENDER_LITERAL(r, "<a href=\"");
    render_attribute(r, &det->href, render_url_escaped);

    if(det->title.text != NULL) {
        RENDER_LITERAL(r, "\" title=\"");
        render_attribute(r, &det->title, render_html_escaped);
    }

    RENDER_LITERAL(r, "\">");
}

static void
render_open_img_span(MD_RENDER_HTML* r, const MD_SPAN_IMG_DETAIL* det)
{
    RENDER_LITERAL(r, "<img src=\"");
    render_attribute(r, &det->src, render_url_escaped);

    RENDER_LITERAL(r, "\" alt=\"");

    r->image_nesting_level++;
}

static void
render_close_img_span(MD_RENDER_HTML* r, const MD_SPAN_IMG_DETAIL* det)
{
    if(det->title.text != NULL) {
        RENDER_LITERAL(r, "\" title=\"");
        render_attribute(r, &det->title, render_html_escaped);
    }

    RENDER_LITERAL(r, "\">");

    r->image_nesting_level--;
}

static void
render_open_wikilink_span(MD_RENDER_HTML* r, const MD_SPAN_WIKILINK_DETAIL* det)
{
    RENDER_LITERAL(r, "<x-wikilink data-target=\"");
    render_attribute(r, &det->target, render_html_escaped);

    RENDER_LITERAL(r, "\">");
}


/**************************************
 ***  HTML renderer implementation  ***
 **************************************/

static int
enter_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    static const MD_CHAR* head[6] = { "<h1>", "<h2>", "<h3>", "<h4>", "<h5>", "<h6>" };
    MD_RENDER_HTML* r = (MD_RENDER_HTML*) userdata;

    switch(type) {
        case MD_BLOCK_DOC:      /* noop */ break;
        case MD_BLOCK_QUOTE:    RENDER_LITERAL(r, "<blockquote>\n"); break;
        case MD_BLOCK_UL:       RENDER_LITERAL(r, "<ul>\n"); break;
        case MD_BLOCK_OL:       render_open_ol_block(r, (const MD_BLOCK_OL_DETAIL*)detail); break;
        case MD_BLOCK_LI:       render_open_li_block(r, (const MD_BLOCK_LI_DETAIL*)detail); break;
        case MD_BLOCK_HR:       RENDER_LITERAL(r, "<hr>\n"); break;
        case MD_BLOCK_H:        RENDER_LITERAL(r, head[((MD_BLOCK_H_DETAIL*)detail)->level - 1]); break;
        case MD_BLOCK_CODE:     render_open_code_block(r, (const MD_BLOCK_CODE_DETAIL*) detail); break;
        case MD_BLOCK_HTML:     /* noop */ break;
        case MD_BLOCK_P:        RENDER_LITERAL(r, "<p>"); break;
        case MD_BLOCK_TABLE:    RENDER_LITERAL(r, "<table>\n"); break;
        case MD_BLOCK_THEAD:    RENDER_LITERAL(r, "<thead>\n"); break;
        case MD_BLOCK_TBODY:    RENDER_LITERAL(r, "<tbody>\n"); break;
        case MD_BLOCK_TR:       RENDER_LITERAL(r, "<tr>\n"); break;
        case MD_BLOCK_TH:       render_open_td_block(r, "th", (MD_BLOCK_TD_DETAIL*)detail); break;
        case MD_BLOCK_TD:       render_open_td_block(r, "td", (MD_BLOCK_TD_DETAIL*)detail); break;
    }

    return 0;
}

static int
leave_block_callback(MD_BLOCKTYPE type, void* detail, void* userdata)
{
    static const MD_CHAR* head[6] = { "</h1>\n", "</h2>\n", "</h3>\n", "</h4>\n", "</h5>\n", "</h6>\n" };
    MD_RENDER_HTML* r = (MD_RENDER_HTML*) userdata;

    switch(type) {
        case MD_BLOCK_DOC:      /*noop*/ break;
        case MD_BLOCK_QUOTE:    RENDER_LITERAL(r, "</blockquote>\n"); break;
        case MD_BLOCK_UL:       RENDER_LITERAL(r, "</ul>\n"); break;
        case MD_BLOCK_OL:       RENDER_LITERAL(r, "</ol>\n"); break;
        case MD_BLOCK_LI:       RENDER_LITERAL(r, "</li>\n"); break;
        case MD_BLOCK_HR:       /*noop*/ break;
        case MD_BLOCK_H:        RENDER_LITERAL(r, head[((MD_BLOCK_H_DETAIL*)detail)->level - 1]); break;
        case MD_BLOCK_CODE:     RENDER_LITERAL(r, "</code></pre>\n"); break;
        case MD_BLOCK_HTML:     /* noop */ break;
        case MD_BLOCK_P:        RENDER_LITERAL(r, "</p>\n"); break;
        case MD_BLOCK_TABLE:    RENDER_LITERAL(r, "</table>\n"); break;
        case MD_BLOCK_THEAD:    RENDER_LITERAL(r, "</thead>\n"); break;
        case MD_BLOCK_TBODY:    RENDER_LITERAL(r, "</tbody>\n"); break;
        case MD_BLOCK_TR:       RENDER_LITERAL(r, "</tr>\n"); break;
        case MD_BLOCK_TH:       RENDER_LITERAL(r, "</th>\n"); break;
        case MD_BLOCK_TD:       RENDER_LITERAL(r, "</td>\n"); break;
    }

    return 0;
}

static int
enter_span_callback(MD_SPANTYPE type, void* detail, void* userdata)
{
    MD_RENDER_HTML* r = (MD_RENDER_HTML*) userdata;

    if(r->image_nesting_level > 0) {
        /* We are inside an image, i.e. rendering the ALT attribute of
         * <IMG> tag. */
        return 0;
    }

    switch(type) {
        case MD_SPAN_EM:                RENDER_LITERAL(r, "<em>"); break;
        case MD_SPAN_STRONG:            RENDER_LITERAL(r, "<strong>"); break;
        case MD_SPAN_A:                 render_open_a_span(r, (MD_SPAN_A_DETAIL*) detail); break;
        case MD_SPAN_IMG:               render_open_img_span(r, (MD_SPAN_IMG_DETAIL*) detail); break;
        case MD_SPAN_CODE:              RENDER_LITERAL(r, "<code>"); break;
        case MD_SPAN_DEL:               RENDER_LITERAL(r, "<del>"); break;
        case MD_SPAN_LATEXMATH:         RENDER_LITERAL(r, "<x-equation>"); break;
        case MD_SPAN_LATEXMATH_DISPLAY: RENDER_LITERAL(r, "<x-equation type=\"display\">"); break;
        case MD_SPAN_WIKILINK:          render_open_wikilink_span(r, (MD_SPAN_WIKILINK_DETAIL*) detail); break;
    }

    return 0;
}

static int
leave_span_callback(MD_SPANTYPE type, void* detail, void* userdata)
{
    MD_RENDER_HTML* r = (MD_RENDER_HTML*) userdata;

    if(r->image_nesting_level > 0) {
        /* We are inside an image, i.e. rendering the ALT attribute of
         * <IMG> tag. */
        if(r->image_nesting_level == 1  &&  type == MD_SPAN_IMG)
            render_close_img_span(r, (MD_SPAN_IMG_DETAIL*) detail);
        return 0;
    }

    switch(type) {
        case MD_SPAN_EM:                RENDER_LITERAL(r, "</em>"); break;
        case MD_SPAN_STRONG:            RENDER_LITERAL(r, "</strong>"); break;
        case MD_SPAN_A:                 RENDER_LITERAL(r, "</a>"); break;
        case MD_SPAN_IMG:               /*noop, handled above*/ break;
        case MD_SPAN_CODE:              RENDER_LITERAL(r, "</code>"); break;
        case MD_SPAN_DEL:               RENDER_LITERAL(r, "</del>"); break;
        case MD_SPAN_LATEXMATH:         /*fall through*/
        case MD_SPAN_LATEXMATH_DISPLAY: RENDER_LITERAL(r, "</x-equation>"); break;
        case MD_SPAN_WIKILINK:          RENDER_LITERAL(r, "</x-wikilink>"); break;
    }

    return 0;
}

static int
text_callback(MD_TEXTTYPE type, const MD_CHAR* text, MD_SIZE size, void* userdata)
{
    MD_RENDER_HTML* r = (MD_RENDER_HTML*) userdata;

    switch(type) {
        case MD_TEXT_NULLCHAR:  render_utf8_codepoint(r, 0x0000, render_text); break;
        case MD_TEXT_BR:        RENDER_LITERAL(r, (r->image_nesting_level == 0 ? "<br>\n" : " ")); break;
        case MD_TEXT_SOFTBR:    RENDER_LITERAL(r, (r->image_nesting_level == 0 ? "\n" : " ")); break;
        case MD_TEXT_HTML:      render_text(r, text, size); break;
        case MD_TEXT_ENTITY:    render_entity(r, text, size, render_html_escaped); break;
        default:                render_html_escaped(r, text, size); break;
    }

    return 0;
}

static void
debug_log_callback(const char* msg, void* userdata)
{
    MD_RENDER_HTML* r = (MD_RENDER_HTML*) userdata;
    if(r->flags & MD_RENDER_FLAG_DEBUG)
        fprintf(stderr, "MD4C: %s\n", msg);
}

int
md_render_html(const MD_CHAR* input, MD_SIZE input_size,
               void (*process_output)(const MD_CHAR*, MD_SIZE, void*),
               void* userdata, unsigned parser_flags, unsigned renderer_flags)
{
    MD_RENDER_HTML render = { process_output, userdata, renderer_flags, 0, { 0 } };

    MD_PARSER parser = {
        0,
        parser_flags,
        enter_block_callback,
        leave_block_callback,
        enter_span_callback,
        leave_span_callback,
        text_callback,
        debug_log_callback,
        NULL
    };

    render.escape_map[(unsigned char)'"'] = 1;
    render.escape_map[(unsigned char)'&'] = 1;
    render.escape_map[(unsigned char)'<'] = 1;
    render.escape_map[(unsigned char)'>'] = 1;

    return md_parse(input, input_size, &parser, (void*) &render);
}

