// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "load.h"
#include "utf16be.h"
#include <dpcommon/conversions.h>
#include <dpcommon/input.h>
#include <dpengine/canvas_state.h>
#include <dpengine/image.h>
#include <dpengine/layer_content.h>
#include <dpengine/layer_group.h>
#include <dpengine/layer_list.h>
#include <dpengine/layer_props.h>
#include <dpengine/layer_props_list.h>
#include <dpmsg/blend_mode.h>
}
#include <Psd.h>
#include <PsdPlatform.h>
// The stuff above needs to come first.
#include <PsdBlendMode.h>
#include <PsdChannel.h>
#include <PsdChannelType.h>
#include <PsdColorMode.h>
#include <PsdDocument.h>
#include <PsdFile.h>
#include <PsdLayer.h>
#include <PsdLayerMaskSection.h>
#include <PsdLayerType.h>
#include <PsdMallocAllocator.h>
#include <PsdMemoryUtil.h>
#include <PsdParseDocument.h>
#include <PsdParseLayerMaskSection.h>
#include <utility>
#include <vector>

// PSD SDK provides an asynchronous file interface by default, but that's way
// slower than just doing it synchronously.
class DP_PsdFile final : public psd::File {
  public:
    DP_PsdFile(psd::Allocator *allocator)
        : psd::File(allocator), m_input(nullptr)
    {
    }

    ~DP_PsdFile() override
    {
        DP_input_free(m_input);
    }

    DP_PsdFile(const DP_PsdFile &) = delete;
    DP_PsdFile(DP_PsdFile &&) = delete;
    DP_PsdFile &operator=(const DP_PsdFile &) = delete;
    DP_PsdFile &operator=(DP_PsdFile &&) = delete;

  private:
    bool DoOpenRead(void *user) override
    {
        m_input = static_cast<DP_Input *>(user);
        return m_input != nullptr;
    }

    bool DoClose(void) override
    {
        if (m_input) {
            DP_input_free(m_input);
            m_input = nullptr;
            return true;
        }
        else {
            return false;
        }
    }

    ReadOperation DoRead(void *buffer, uint32_t count,
                         uint64_t position) override
    {
        if (count != 0) {
            DP_ASSERT(buffer);
            if (!DP_input_seek(m_input, DP_uint64_to_size(position))) {
                return nullptr;
            }

            bool error;
            size_t size = DP_uint32_to_size(count);
            size_t read = DP_input_read(m_input, buffer, size, &error);
            if (error) {
                DP_warn("Error reading %u bytes from PSD: %s", count,
                        DP_error());
                return nullptr;
            }
            else if (read != count) {
                DP_warn("Expected to read %zu bytes from PSD, but got %zu",
                        size, read);
                return nullptr;
            }
            else {
                return this;
            }
        }
        else {
            return this;
        }
    }

    bool DoWaitForRead(ReadOperation &) override
    {
        return true;
    }

    uint64_t DoGetSize(void) const override
    {
        return DP_size_to_uint64(DP_input_length(m_input, nullptr));
    }

    DP_Input *m_input;
};

struct DP_PsdLayerPair {
    DP_TransientLayerProps *tlp;
    union {
        DP_TransientLayerContent *lc;
        DP_TransientLayerGroup *lg;
    } t;
};

static void assign_load_result(DP_LoadResult *out_result, DP_LoadResult result)
{
    if (out_result) {
        *out_result = result;
    }
}

static bool set_layer_title(void *user, const char *title, size_t title_length)
{
    DP_TransientLayerProps *tlp = static_cast<DP_TransientLayerProps *>(user);
    DP_transient_layer_props_title_set(tlp, title, title_length);
    return true;
}

static int extract_blend_mode(uint32_t key)
{
    psd::blendMode::Enum mode = psd::blendMode::KeyToEnum(key);
    switch (mode) {
    case psd::blendMode::NORMAL:
        return DP_BLEND_MODE_NORMAL;
    case psd::blendMode::DARKEN:
        return DP_BLEND_MODE_DARKEN_ALPHA;
    case psd::blendMode::MULTIPLY:
        return DP_BLEND_MODE_MULTIPLY_ALPHA;
    case psd::blendMode::COLOR_BURN:
        return DP_BLEND_MODE_BURN_ALPHA;
    case psd::blendMode::LINEAR_BURN:
        return DP_BLEND_MODE_LINEAR_BURN_ALPHA;
    case psd::blendMode::LIGHTEN:
        return DP_BLEND_MODE_LIGHTEN_ALPHA;
    case psd::blendMode::SCREEN:
        return DP_BLEND_MODE_SCREEN_ALPHA;
    case psd::blendMode::COLOR_DODGE:
        return DP_BLEND_MODE_DODGE_ALPHA;
    case psd::blendMode::LINEAR_DODGE:
        return DP_BLEND_MODE_ADD_ALPHA;
    case psd::blendMode::OVERLAY:
        return DP_BLEND_MODE_OVERLAY_ALPHA;
    case psd::blendMode::SOFT_LIGHT:
        return DP_BLEND_MODE_SOFT_LIGHT_ALPHA;
    case psd::blendMode::HARD_LIGHT:
        return DP_BLEND_MODE_HARD_LIGHT_ALPHA;
    case psd::blendMode::LINEAR_LIGHT:
        return DP_BLEND_MODE_LINEAR_LIGHT_ALPHA;
    case psd::blendMode::SUBTRACT:
        return DP_BLEND_MODE_SUBTRACT_ALPHA;
    case psd::blendMode::DIVIDE:
        return DP_BLEND_MODE_DIVIDE_ALPHA;
    case psd::blendMode::HUE:
        return DP_BLEND_MODE_HUE_ALPHA;
    case psd::blendMode::SATURATION:
        return DP_BLEND_MODE_SATURATION_ALPHA;
    case psd::blendMode::COLOR:
        return DP_BLEND_MODE_COLOR_ALPHA;
    case psd::blendMode::LUMINOSITY:
        return DP_BLEND_MODE_LUMINOSITY_ALPHA;
    default:
        DP_warn("Unhandled PSD blend mode '%s'",
                psd::blendMode::ToString(mode));
        return DP_BLEND_MODE_NORMAL;
    }
}

static void apply_layer_props(DP_TransientLayerProps *tlp, psd::Layer *layer)
{
    if (!DP_utf16be_to_utf8(layer->utf16Name, set_layer_title, tlp)) {
        DP_transient_layer_props_title_set(tlp, layer->name.c_str(),
                                           layer->name.GetLength());
    }
    DP_transient_layer_props_opacity_set(tlp,
                                         DP_channel8_to_15(layer->opacity));
    DP_transient_layer_props_blend_mode_set(
        tlp, extract_blend_mode(layer->blendModeKey));
    if (!layer->isVisible) {
        DP_transient_layer_props_hidden_set(tlp, true);
    }
    if (DP_transient_layer_props_children_noinc(tlp) && layer->isPassThrough) {
        DP_transient_layer_props_isolated_set(tlp, false);
    }
    if (layer->clipping == 1) {
        DP_transient_layer_props_clip_set(tlp, true);
    }
}

static void combine8(int size, DP_Pixel8 *pixels, const uint8_t *a,
                     const uint8_t *r, const uint8_t *g, const uint8_t *b)
{
    for (int i = 0; i < size; ++i) {
        DP_UPixel8 pixel;
        pixel.bytes.a = a ? a[i] : 0xff;
        pixel.bytes.r = r ? r[i] : 0;
        pixel.bytes.g = g ? g[i] : 0;
        pixel.bytes.b = b ? b[i] : 0;
        pixels[i] = DP_pixel8_premultiply(pixel);
    }
}

static void extract_layer_pixels(psd::Document *document, psd::Layer *layer,
                                 DP_TransientLayerContent *tlc)
{
    int left = layer->left;
    int top = layer->top;
    int right = layer->right;
    int bottom = layer->bottom;
    if (left >= right || top >= bottom) {
        return;
    }

    unsigned int channel_count = layer->channelCount;
    void *a = nullptr;
    void *r = nullptr;
    void *b = nullptr;
    void *g = nullptr;
    for (unsigned int i = 0; i < channel_count; ++i) {
        psd::Channel *channel = &layer->channels[i];
        if (channel->data) {
            switch (channel->type) {
            case psd::channelType::TRANSPARENCY_MASK:
                if (!a) {
                    a = channel->data;
                }
                break;
            case psd::channelType::R:
                if (!r) {
                    r = channel->data;
                }
                break;
            case psd::channelType::G:
                if (!g) {
                    g = channel->data;
                }
                break;
            case psd::channelType::B:
                if (!b) {
                    b = channel->data;
                }
                break;
            default:
                break;
            }
        }
    }
    if (a || r || g || b) {
        int width = right - left;
        int height = bottom - top;
        int size = width * height;
        DP_Image *img = DP_image_new(width, height);
        DP_Pixel8 *pixels = DP_image_pixels(img);
        switch (document->bitsPerChannel) {
        case 8:
            combine8(size, pixels, static_cast<uint8_t *>(a),
                     static_cast<uint8_t *>(r), static_cast<uint8_t *>(g),
                     static_cast<uint8_t *>(b));
            break;
        default:
            DP_UNREACHABLE();
        }
        DP_transient_layer_content_put_image(tlc, 1, DP_BLEND_MODE_REPLACE,
                                             left, top, img);
        DP_image_free(img);
    }
}

static DP_PsdLayerPair extract_layer_content(psd::Document *document,
                                             int &layer_id, psd::Layer *layer)
{
    DP_PsdLayerPair p;
    p.tlp = DP_transient_layer_props_new_init(layer_id++, false);
    apply_layer_props(p.tlp, layer);

    p.t.lc = DP_transient_layer_content_new_init(
        int(document->width), int(document->height), nullptr);
    extract_layer_pixels(document, layer, p.t.lc);

    return p;
}

static std::vector<DP_PsdLayerPair> extract_layers_recursive(
    psd::Document *document, psd::File *file, psd::Allocator *allocator,
    psd::LayerMaskSection *section, unsigned int &i, int &layer_id);

static std::pair<DP_TransientLayerPropsList *, DP_TransientLayerList *>
build_layer_lists(const std::vector<DP_PsdLayerPair> &layers)
{
    int child_count = int(layers.size());
    DP_TransientLayerPropsList *tlpl =
        DP_transient_layer_props_list_new_init(child_count);
    DP_TransientLayerList *tll = DP_transient_layer_list_new_init(child_count);

    for (int i = 0; i < child_count; ++i) {
        const DP_PsdLayerPair &p =
            layers[std::vector<DP_PsdLayerPair>::size_type(i)];
        DP_TransientLayerProps *child_tlp = p.tlp;
        DP_transient_layer_props_list_set_noinc(
            tlpl, reinterpret_cast<DP_LayerProps *>(child_tlp), i);
        if (DP_transient_layer_props_children_noinc(child_tlp)) {
            DP_transient_layer_list_set_group_noinc(
                tll, reinterpret_cast<DP_LayerGroup *>(p.t.lg), i);
        }
        else {
            DP_transient_layer_list_set_content_noinc(
                tll, reinterpret_cast<DP_LayerContent *>(p.t.lc), i);
        }
    }

    return {tlpl, tll};
}

static DP_PsdLayerPair extract_layer_group(psd::Document *document,
                                           psd::File *file,
                                           psd::Allocator *allocator,
                                           psd::LayerMaskSection *section,
                                           unsigned int &i, int &layer_id)
{
    int group_id = layer_id++;
    std::vector<DP_PsdLayerPair> layers = extract_layers_recursive(
        document, file, allocator, section, i, layer_id);

    auto [tlpl, tll] = build_layer_lists(layers);

    DP_PsdLayerPair p;
    p.t.lg = DP_transient_layer_group_new_init_with_transient_children_noinc(
        int(document->width), int(document->height), tll);
    p.tlp = DP_transient_layer_props_new_init_with_transient_children_noinc(
        group_id, tlpl);

    psd::Layer *layer = &section->layers[i];
    apply_layer_props(p.tlp, layer);
    ++i;

    return p;
}

static std::vector<DP_PsdLayerPair> extract_layers_recursive(
    psd::Document *document, psd::File *file, psd::Allocator *allocator,
    psd::LayerMaskSection *section, unsigned int &i, int &layer_id)
{
    std::vector<DP_PsdLayerPair> layers;
    while (i < section->layerCount) {
        psd::Layer *layer = &section->layers[i];
        uint32_t type = layer->type;
        if (type == psd::layerType::SECTION_DIVIDER) {
            // Start of a new group.
            ++i;
            layers.push_back(extract_layer_group(document, file, allocator,
                                                 section, i, layer_id));
        }
        else if (type == psd::layerType::OPEN_FOLDER
                 || type == psd::layerType::CLOSED_FOLDER) {
            // End of this group.
            break;
        }
        else {
            // Regular layer.
            ++i;
            psd::ExtractLayer(document, file, allocator, layer);
            layers.push_back(extract_layer_content(document, layer_id, layer));
        }
    }
    return layers;
}

static DP_TransientCanvasState *extract_layers(psd::Document *document,
                                               psd::File *file,
                                               psd::Allocator *allocator)
{
    psd::LayerMaskSection *section =
        psd::ParseLayerMaskSection(document, file, allocator);
    if (!section) {
        DP_error_set("Failed to read layers section from PSD");
        return nullptr;
    }

    unsigned int i = 0;
    int layer_id = 0x100;
    std::vector<DP_PsdLayerPair> layers = extract_layers_recursive(
        document, file, allocator, section, i, layer_id);
    auto [tlpl, tll] = build_layer_lists(layers);

    psd::DestroyLayerMaskSection(section, allocator);

    DP_TransientCanvasState *tcs = DP_transient_canvas_state_new_init();
    DP_transient_canvas_state_width_set(tcs, int(document->width));
    DP_transient_canvas_state_height_set(tcs, int(document->height));
    DP_transient_canvas_state_transient_layer_props_set_noinc(tcs, tlpl);
    DP_transient_canvas_state_transient_layers_set_noinc(tcs, tll);
    return tcs;
}

extern "C" DP_CanvasState *DP_load_psd(DP_DrawContext *dc, DP_Input *input,
                                       DP_LoadResult *out_result)
{
    psd::MallocAllocator allocator;
    DP_PsdFile file(&allocator);
    bool opened = file.OpenRead(input);
    if (!opened) {
        DP_error_set("Failed to open PSD file");
        assign_load_result(out_result, DP_LOAD_RESULT_OPEN_ERROR);
        return nullptr;
    }

    psd::Document *document = psd::CreateDocument(&file, &allocator);
    if (!document) {
        DP_error_set("Failed to parse PSD document");
        assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
        file.Close();
        return nullptr;
    }

    unsigned int bpc = document->bitsPerChannel;
    if (bpc != 8) {
        DP_error_set("PSD bits per channel %u is unsupported", bpc);
        assign_load_result(out_result,
                           DP_LOAD_RESULT_UNSUPPORTED_PSD_BITS_PER_CHANNEL);
        psd::DestroyDocument(document, &allocator);
        file.Close();
        return nullptr;
    }

    unsigned int color_mode = document->colorMode;
    if (color_mode != psd::colorMode::RGB) {
        DP_error_set("PSD color mode %u is unsupported", color_mode);
        assign_load_result(out_result,
                           DP_LOAD_RESULT_UNSUPPORTED_PSD_COLOR_MODE);
        psd::DestroyDocument(document, &allocator);
        file.Close();
        return nullptr;
    }

    unsigned int width = document->width;
    unsigned int height = document->height;
    if (width > UINT16_MAX || height > UINT16_MAX) {
        DP_error_set("PSD dimensions %ux%u too large", width, height);
        assign_load_result(out_result, DP_LOAD_RESULT_IMAGE_TOO_LARGE);
        psd::DestroyDocument(document, &allocator);
        file.Close();
        return nullptr;
    }

    DP_TransientCanvasState *tcs = extract_layers(document, &file, &allocator);
    psd::DestroyDocument(document, &allocator);
    file.Close();
    if (tcs) {
        assign_load_result(out_result, DP_LOAD_RESULT_SUCCESS);
        DP_transient_canvas_state_intuit_background(tcs);
        DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
        return DP_transient_canvas_state_persist(tcs);
    }
    else {
        assign_load_result(out_result, DP_LOAD_RESULT_READ_ERROR);
        return nullptr;
    }
}
