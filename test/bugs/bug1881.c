/*
 * Regression test for issue #1881: a RenderAddGlyphs request carrying two
 * glyphs with identical content (same SHA1) but different glyph ids desynced
 * the global glyph hash accounting, eventually tearing down the shared hash
 * while a glyphset still referenced it and crashing the server in FreeGlyph
 * with a NULL hashSet.
 *
 * The sequence below reproduces the crash on an unpatched server:
 *   1. glyphset gs2 gets one unique glyph (global tableEntries = 1)
 *   2. glyphset gs1 gets two glyphs with the same bitmap+info, different ids;
 *      freeing the redundant duplicate evicts the live glyph and miscounts
 *   3. freeing gs2 drives tableEntries to 0 and frees the shared global hash
 *      while gs1 still holds a glyph of the same format depth
 *   4. freeing gs1's glyphs dereferences the now-NULL hashSet
 *
 * A round trip after the sequence detects whether the server survived.
 * Exit status 0 = server alive (fixed), non-zero = crash or error (buggy).
 */

#include <stdio.h>
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/render.h>
#include <xcb/xcb_renderutil.h>

#define GS2_GID 100
#define GS1_GID_A 200
#define GS1_GID_B 201

static int
add_glyphs(xcb_connection_t *c, xcb_render_glyphset_t gs,
           uint32_t nglyphs, const uint32_t *ids,
           const xcb_render_glyphinfo_t *infos,
           uint32_t data_len, const uint8_t *data)
{
    xcb_void_cookie_t ck =
        xcb_render_add_glyphs_checked(c, gs, nglyphs, ids, infos,
                                      data_len, data);
    xcb_generic_error_t *err = xcb_request_check(c, ck);
    if (err) {
        fprintf(stderr, "AddGlyphs failed: error code %d\n", err->error_code);
        free(err);
        return -1;
    }
    return 0;
}

int
main(void)
{
    xcb_connection_t *c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(c)) {
        fprintf(stderr, "cannot connect to X server\n");
        return 2;
    }

    xcb_render_query_version_cookie_t vcookie =
        xcb_render_query_version(c, 0, 11);
    xcb_render_query_version_reply_t *ver =
        xcb_render_query_version_reply(c, vcookie, NULL);
    if (!ver) {
        fprintf(stderr, "RENDER extension not available\n");
        xcb_disconnect(c);
        return 2;
    }
    free(ver);

    const xcb_render_query_pict_formats_reply_t *formats =
        xcb_render_util_query_formats(c);
    xcb_render_pictforminfo_t *a8 =
        xcb_render_util_find_standard_format(formats, XCB_PICT_STANDARD_A_8);
    if (!a8) {
        fprintf(stderr, "no A8 picture format\n");
        xcb_disconnect(c);
        return 2;
    }

    xcb_render_glyphinfo_t info = {
        .width = 4, .height = 1, .x = 0, .y = 0, .x_off = 4, .y_off = 0,
    };
    const uint8_t bits_unique[4] = { 0x11, 0x22, 0x33, 0x44 };
    const uint8_t bits_dup[8] =
        { 0xAA, 0xBB, 0xCC, 0xDD, 0xAA, 0xBB, 0xCC, 0xDD };

    xcb_render_glyphset_t gs2 = xcb_generate_id(c);
    xcb_render_create_glyph_set(c, gs2, a8->id);
    uint32_t gs2_ids[1] = { GS2_GID };
    xcb_render_glyphinfo_t gs2_infos[1] = { info };
    if (add_glyphs(c, gs2, 1, gs2_ids, gs2_infos,
                   sizeof(bits_unique), bits_unique) != 0)
        goto fail;

    xcb_render_glyphset_t gs1 = xcb_generate_id(c);
    xcb_render_create_glyph_set(c, gs1, a8->id);
    uint32_t gs1_ids[2] = { GS1_GID_A, GS1_GID_B };
    xcb_render_glyphinfo_t gs1_infos[2] = { info, info };
    if (add_glyphs(c, gs1, 2, gs1_ids, gs1_infos,
                   sizeof(bits_dup), bits_dup) != 0)
        goto fail;

    xcb_render_free_glyph_set(c, gs2);

    uint32_t free_a[1] = { GS1_GID_A };
    uint32_t free_b[1] = { GS1_GID_B };
    xcb_render_free_glyphs(c, gs1, 1, free_a);
    xcb_render_free_glyphs(c, gs1, 1, free_b);
    xcb_render_free_glyph_set(c, gs1);

    xcb_get_input_focus_reply_t *focus =
        xcb_get_input_focus_reply(c, xcb_get_input_focus(c), NULL);
    if (!focus || xcb_connection_has_error(c)) {
        fprintf(stderr, "server died after duplicate-glyph sequence\n");
        free(focus);
        xcb_disconnect(c);
        return 1;
    }
    free(focus);

    printf("PASS: server survived duplicate-glyph sequence\n");
    xcb_disconnect(c);
    return 0;

 fail:
    xcb_disconnect(c);
    return 2;
}
