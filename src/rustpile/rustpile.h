#ifndef DP_RUSTPILE_FFI_H
#define DP_RUSTPILE_FFI_H

/* Generated with cbindgen, do not edit manually. Run bindgen.sh to regenerate this file. */

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace rustpile {

static const uint8_t JoinMessage_FLAGS_AUTH = 1;

static const uint8_t JoinMessage_FLAGS_BOT = 4;

static const uint8_t LayerAttributesMessage_FLAGS_CENSOR = 1;

static const uint8_t LayerCreateMessage_FLAGS_COPY = 1;

static const uint8_t LayerAttributesMessage_FLAGS_FIXED = 2;

static const uint8_t LayerCreateMessage_FLAGS_INSERT = 2;

static const uint8_t JoinMessage_FLAGS_MOD = 2;

static const uint8_t AnnotationEditMessage_FLAGS_PROTECT = 1;

static const uint8_t AnnotationEditMessage_FLAGS_VALIGN_BOTTOM = 4;

static const uint8_t AnnotationEditMessage_FLAGS_VALIGN_CENTER = 2;

static const uintptr_t DrawDabsClassicMessage_MAX_CLASSICDABS = 10920;

static const uintptr_t DrawDabsPixelMessage_MAX_PIXELDABS = 16380;

static const uint8_t ChatMessage_OFLAGS_ACTION = 2;

static const uint8_t ChatMessage_OFLAGS_PIN = 4;

static const uint8_t ChatMessage_OFLAGS_SHOUT = 1;

static const uint8_t ChatMessage_TFLAGS_BYPASS = 1;

enum class Blendmode : uint8_t {
  Erase = 0,
  Normal,
  Multiply,
  Divide,
  Burn,
  Dodge,
  Darken,
  Lighten,
  Subtract,
  Add,
  Recolor,
  Behind,
  ColorErase,
  Replace = 255,
};

enum class BrushPreviewShape {
  Stroke,
  Line,
  Rectangle,
  Ellipse,
  FloodFill,
  FloodErase,
};

enum class ClassicBrushShape : uint8_t {
  RoundPixel,
  SquarePixel,
  RoundSoft,
};

enum class LayerViewMode {
  Normal,
  Solo,
  Onionskin,
};

/// Feature access tiers
enum class Tier : uint8_t {
  Operator,
  Trusted,
  Authenticated,
  Guest,
};

enum class VAlign {
  Top,
  Center,
  Bottom,
};

/// A snapshot of annotations for updating their GUI representations
///
/// This struct is thread-safe and can be passed to the main thread
/// from the paint engine thread.
struct Annotations;

struct BrushEngine;

struct BrushPreview;

struct MessageWriter;

/// The paint engine
struct PaintEngine;

using AnnotationID = uint16_t;

struct Rectangle {
  int32_t x;
  int32_t y;
  int32_t w;
  int32_t h;
};

struct Color {
  float r;
  float g;
  float b;
  float a;
};
static const Color Color_TRANSPARENT = Color{ /* .r = */ 0.0, /* .g = */ 0.0, /* .b = */ 0.0, /* .a = */ 0.0 };
static const Color Color_BLACK = Color{ /* .r = */ 0.0, /* .g = */ 0.0, /* .b = */ 0.0, /* .a = */ 1.0 };
static const Color Color_WHITE = Color{ /* .r = */ 1.0, /* .g = */ 1.0, /* .b = */ 1.0, /* .a = */ 1.0 };

using UpdateAnnotationCallback = void(*)(void *ctx, AnnotationID id, const char *text, uintptr_t textlen, Rectangle rect, Color background, bool protect, VAlign valign);

struct Range {
  float min;
  float max;
};

/// The parameters of a classic soft and pixel Drawpile brushes.
struct ClassicBrush {
  /// The diameter of the brush
  Range size;
  /// Brush hardness (ignored for Pixel shapes)
  Range hardness;
  /// Brush opacity (in indirect mode, this is the opacity of the entire stroke)
  Range opacity;
  /// Smudging factor (1.0 means color is entirely replaced by picked up color)
  Range smudge;
  /// Distance between dabs in diameters.
  float spacing;
  /// Color pickup frequency. <=1 means color is picked up at every dab.
  int32_t resmudge;
  /// Initial brush color
  Color color;
  /// Brush dab shape
  ClassicBrushShape shape;
  /// Blending mode
  Blendmode mode;
  /// Draw directly without using a temporary layer
  bool incremental;
  /// Pick initial color from the layer
  bool colorpick;
  /// Apply pressure to size
  bool size_pressure;
  /// Apply pressure to hardness
  bool hardness_pressure;
  /// Apply pressure to opacity
  bool opacity_pressure;
  /// Apply pressure to smudging
  bool smudge_pressure;
};

/// The regular layer ID type is used for layers that can be accessed
/// via the protocol.
using LayerID = uint16_t;

using UserID = uint8_t;

using NotifyChangesCallback = void(*)(void *ctx, Rectangle area);

struct Size {
  int32_t width;
  int32_t height;
};

using NotifyResizeCallback = void(*)(void *ctx, int32_t x_offset, int32_t y_offset, Size old_size);

/// Layer's properties for updating a layer list in the GUI
struct LayerInfo {
  const uint8_t *title;
  int32_t titlelen;
  float opacity;
  int32_t id;
  bool hidden;
  bool censored;
  bool fixed;
  Blendmode blendmode;
};

using NotifyLayerListCallback = void(*)(void *ctx, const LayerInfo *layers, uintptr_t count);

using NotifyAnnotationsCallback = void(*)(void *ctx, Annotations *annotations);

using NotifyCursorCallback = void(*)(void *ctx, UserID user, uint16_t layer, int32_t x, int32_t y);

using NotifyPlaybackCallback = void(*)(void *ctx, int64_t pos, uint32_t interval);

using JoinCallback = void(*)(void *ctx, UserID user, uint8_t flags, const uint8_t *name, uintptr_t name_len, const uint8_t *avatar, uintptr_t avatar_len);

using LeaveCallback = void(*)(void *ctx, UserID user);

using ChatCallback = void(*)(void *ctx, UserID sender, UserID recipient, uint8_t tflags, uint8_t oflags, const uint8_t *message, uintptr_t message_len);

using LaserCallback = void(*)(void *ctx, UserID user, uint8_t persistence, uint32_t color);

using DefaultLayerCallback = void(*)(void *ctx, LayerID layer);

using AclChange = uint32_t;

using AclChangeCallback = void(*)(void *ctx, AclChange changes);

using RecordingStateCallback = void(*)(void *ctx, bool recording);

/// The result of an "annotation at point" query
struct AnnotationAt {
  /// ID of the annotation at the queried point.
  /// Will be zero if none was found
  AnnotationID id;
  /// The shape of the annotation
  Rectangle rect;
  /// The annotation's protected bit
  bool protect;
};

/// Bitfield for storing a set of users (IDs range from 0..255)
using UserBits = uint8_t[32];

/// Set of general user related permission bits
struct UserACLs {
  UserBits operators;
  UserBits trusted;
  UserBits authenticated;
  UserBits locked;
  bool all_locked;
};

/// Layer specific permissions
struct LayerACL {
  /// General layer-wide lock
  bool locked;
  /// Layer general access tier
  Tier tier;
  /// Exclusive access granted to these users
  UserBits exclusive;
};

struct FeatureTiers {
  /// Use of the PutImage command (covers cut&paste, move with transform, etc.)
  Tier put_image;
  /// Selection moving (without transformation)
  Tier move_rect;
  /// Canvas resize
  Tier resize;
  /// Canvas background changing
  Tier background;
  /// Permission to edit any layer's properties and to reorganize them
  Tier edit_layers;
  /// Permission to create and edit own layers
  Tier own_layers;
  /// Permission to create new annotations
  Tier create_annotation;
  /// Permission to use the laser pointer tool
  Tier laser;
  /// Permission to use undo/redo
  Tier undo;
};

extern "C" {

void rustpile_init();

void qt_log_handler(int32_t loglevel, const uint16_t *message, uintptr_t message_len);

void annotations_get_all(const Annotations *annotations,
                         void *ctx,
                         UpdateAnnotationCallback callback);

void annotations_free(Annotations *annotations);

BrushEngine *brushengine_new();

void brushengine_free(BrushEngine *be);

void brushengine_set_classicbrush(BrushEngine *be, const ClassicBrush *brush, uint16_t layer);

void brushengine_stroke_to(BrushEngine *be,
                           float x,
                           float y,
                           float p,
                           const PaintEngine *pe,
                           LayerID layer_id);

void brushengine_end_stroke(BrushEngine *be);

void brushengine_add_offset(BrushEngine *be, float x, float y);

void brushengine_write_dabs(BrushEngine *be, uint8_t user_id, MessageWriter *writer);

BrushPreview *brushpreview_new(uint32_t width, uint32_t height);

void brushpreview_free(BrushPreview *bp);

void brushpreview_render(BrushPreview *bp, const ClassicBrush *brush, BrushPreviewShape shape);

void brushpreview_floodfill(BrushPreview *bp,
                            const Color *color,
                            float tolerance,
                            int32_t expansion,
                            bool fill_under);

void brushpreview_paint(const BrushPreview *bp,
                        void *ctx,
                        void (*paint_func)(void *ctx, int32_t x, int32_t y, const uint8_t *pixels));

MessageWriter *messagewriter_new();

void messagewriter_free(MessageWriter *mw);

const uint8_t *messagewriter_content(const MessageWriter *mw, uintptr_t *len);

void write_servercommand(MessageWriter *writer, UserID ctx, const uint8_t *msg, uintptr_t msg_len);

void write_disconnect(MessageWriter *writer,
                      UserID ctx,
                      uint8_t reason,
                      const uint16_t *message,
                      uintptr_t message_len);

void write_ping(MessageWriter *writer, UserID ctx, bool is_pong);

void write_sessionowner(MessageWriter *writer,
                        UserID ctx,
                        const uint8_t *users,
                        uintptr_t users_len);

void write_chat(MessageWriter *writer,
                UserID ctx,
                uint8_t tflags,
                uint8_t oflags,
                const uint16_t *message,
                uintptr_t message_len);

void write_trusted(MessageWriter *writer, UserID ctx, const uint8_t *users, uintptr_t users_len);

void write_privatechat(MessageWriter *writer,
                       UserID ctx,
                       uint8_t target,
                       uint8_t oflags,
                       const uint16_t *message,
                       uintptr_t message_len);

void write_lasertrail(MessageWriter *writer, UserID ctx, uint32_t color, uint8_t persistence);

void write_movepointer(MessageWriter *writer, UserID ctx, int32_t x, int32_t y);

void write_marker(MessageWriter *writer, UserID ctx, const uint16_t *text, uintptr_t text_len);

void write_useracl(MessageWriter *writer, UserID ctx, const uint8_t *users, uintptr_t users_len);

void write_layeracl(MessageWriter *writer,
                    UserID ctx,
                    uint16_t id,
                    uint8_t flags,
                    const uint8_t *exclusive,
                    uintptr_t exclusive_len);

void write_featureaccess(MessageWriter *writer,
                         UserID ctx,
                         const uint8_t *feature_tiers,
                         uintptr_t feature_tiers_len);

void write_defaultlayer(MessageWriter *writer, UserID ctx, uint16_t id);

void write_undopoint(MessageWriter *writer, UserID ctx);

void write_resize(MessageWriter *writer,
                  UserID ctx,
                  int32_t top,
                  int32_t right,
                  int32_t bottom,
                  int32_t left);

void write_newlayer(MessageWriter *writer,
                    UserID ctx,
                    uint16_t id,
                    uint16_t source,
                    uint32_t fill,
                    uint8_t flags,
                    const uint16_t *name,
                    uintptr_t name_len);

void write_layerattr(MessageWriter *writer,
                     UserID ctx,
                     uint16_t id,
                     uint8_t sublayer,
                     uint8_t flags,
                     uint8_t opacity,
                     uint8_t blend);

void write_retitlelayer(MessageWriter *writer,
                        UserID ctx,
                        uint16_t id,
                        const uint16_t *title,
                        uintptr_t title_len);

void write_layerorder(MessageWriter *writer,
                      UserID ctx,
                      const uint16_t *layers,
                      uintptr_t layers_len);

void write_deletelayer(MessageWriter *writer, UserID ctx, uint16_t id, bool merge);

void write_layervisibility(MessageWriter *writer, UserID ctx, uint16_t id, bool visible);

void write_fillrect(MessageWriter *writer,
                    UserID ctx,
                    uint16_t layer,
                    uint8_t mode,
                    uint32_t x,
                    uint32_t y,
                    uint32_t w,
                    uint32_t h,
                    uint32_t color);

void write_penup(MessageWriter *writer, UserID ctx);

void write_newannotation(MessageWriter *writer,
                         UserID ctx,
                         uint16_t id,
                         int32_t x,
                         int32_t y,
                         uint16_t w,
                         uint16_t h);

void write_reshapeannotation(MessageWriter *writer,
                             UserID ctx,
                             uint16_t id,
                             int32_t x,
                             int32_t y,
                             uint16_t w,
                             uint16_t h);

void write_editannotation(MessageWriter *writer,
                          UserID ctx,
                          uint16_t id,
                          uint32_t bg,
                          uint8_t flags,
                          uint8_t border,
                          const uint16_t *text,
                          uintptr_t text_len);

void write_deleteannotation(MessageWriter *writer, UserID ctx, uint16_t id);

void write_puttile(MessageWriter *writer,
                   UserID ctx,
                   uint16_t layer,
                   uint8_t sublayer,
                   uint16_t col,
                   uint16_t row,
                   uint16_t repeat,
                   const uint8_t *image,
                   uintptr_t image_len);

void write_background(MessageWriter *writer, UserID ctx, const uint8_t *image, uintptr_t image_len);

void write_moverect(MessageWriter *writer,
                    UserID ctx,
                    uint16_t layer,
                    int32_t sx,
                    int32_t sy,
                    int32_t tx,
                    int32_t ty,
                    int32_t w,
                    int32_t h,
                    const uint8_t *mask,
                    uintptr_t mask_len);

void write_undo(MessageWriter *writer, UserID ctx, uint8_t override_user, bool redo);

void write_putimage(MessageWriter *writer,
                    UserID user,
                    LayerID layer,
                    uint32_t x,
                    uint32_t y,
                    uint32_t w,
                    uint32_t h,
                    Blendmode mode,
                    const uint8_t *pixels);

/// Construct a new paint engine with an empty canvas.
PaintEngine *paintengine_new(void *ctx,
                             NotifyChangesCallback changes,
                             NotifyResizeCallback resizes,
                             NotifyLayerListCallback layers,
                             NotifyAnnotationsCallback annotations,
                             NotifyCursorCallback cursors,
                             NotifyPlaybackCallback playback);

/// Delete a paint engine instance and wait for its thread to finish
void paintengine_free(PaintEngine *dp);

/// Register callbacks for Meta messages
void paintengine_register_meta_callbacks(PaintEngine *dp,
                                         void *ctx,
                                         JoinCallback join,
                                         LeaveCallback leave,
                                         ChatCallback chat,
                                         LaserCallback laser,
                                         DefaultLayerCallback defaultlayer,
                                         AclChangeCallback aclchange,
                                         RecordingStateCallback recordingstate);

/// Get the current size of the canvas.
Size paintengine_canvas_size(const PaintEngine *dp);

/// Receive one or more messages
/// Returns false if the paint engine thread has panicked
bool paintengine_receive_messages(PaintEngine *dp,
                                  bool local,
                                  const uint8_t *messages,
                                  uintptr_t messages_len);

/// Clean up the paint engine state after disconnecting from a session
bool paintengine_cleanup(PaintEngine *dp);

/// Reset the ACL filter back to local (non-networked) operating mode
void paintengine_reset_acl(PaintEngine *dp, UserID local_user);

/// Get the color of the background tile
///
/// TODO this presently assumes the background tile is always solid.
/// TODO We should support background patterns in the GUI as well.
Color paintengine_background_color(const PaintEngine *dp);

/// Find the next unused annotation ID for the given user
AnnotationID paintengine_get_available_annotation_id(const PaintEngine *dp, UserID user);

/// Find the annotation at the given position
AnnotationAt paintengine_get_annotation_at(const PaintEngine *dp,
                                           int32_t x,
                                           int32_t y,
                                           int32_t expand);

/// Check if the paint engine's content is simple enough to be saved in a flat image
///
/// If any features that requires the OpenRaster file format (such as multiple layers)
/// are used, this will return false.
bool paintengine_is_simple(const PaintEngine *dp);

void paintengine_set_view_mode(PaintEngine *dp, LayerViewMode mode, bool censor);

void paintengine_set_onionskin_opts(PaintEngine *dp,
                                    int32_t skins_below,
                                    int32_t skins_above,
                                    bool tint);

void paintengine_set_active_layer(PaintEngine *dp, LayerID layer_id);

/// Check the given coordinates and return the ID of the user
/// who last touched the tile under it.
/// This will also set that user ID as the canvas highlight ID.
UserID paintengine_inspect_canvas(PaintEngine *dp, int32_t x, int32_t y);

/// Set the canvas inspect target user ID.
/// When set, all tiles last  touched by this use will be highlighted.
/// Setting this to zero switches off inspection mode.
void paintengine_set_highlight_user(PaintEngine *dp, UserID user);

/// Draw a preview brush stroke onto the given layer
///
/// This consumes the content of the brush engine.
void paintengine_preview_brush(PaintEngine *dp, LayerID layer_id, BrushEngine *brushengine);

/// Make a temporary eraser layer to preview a cut operation
void paintengine_preview_cut(PaintEngine *dp,
                             LayerID layer_id,
                             Rectangle rect,
                             const uint8_t *mask);

/// Remove preview brush strokes from the given layer
/// If layer ID is 0, previews from all layers will be removed
void paintengine_remove_preview(PaintEngine *dp, LayerID layer_id);

/// Perform a flood fill operation
///
/// Returns false if the operation fails (e.g. due to size limit.)
bool paintengine_floodfill(PaintEngine *dp,
                           MessageWriter *writer,
                           UserID user_id,
                           LayerID layer_id,
                           int32_t x,
                           int32_t y,
                           Color color,
                           float tolerance,
                           bool sample_merged,
                           uint32_t size_limit,
                           int32_t expansion,
                           bool fill_under);

/// Pick a color from the canvas
///
/// If the given layer ID is 0, color is taken from merged layers
Color paintengine_sample_color(const PaintEngine *dp,
                               int32_t x,
                               int32_t y,
                               LayerID layer_id,
                               int32_t dia);

/// Find the topmost layer at the given coordinates
///
LayerID paintengine_pick_layer(const PaintEngine *dp, int32_t x, int32_t y);

/// Copy layer pixel data to the given buffer
///
/// The rectangle must be contained within the layer bounds.
/// The size if the buffer must be rect.w * rect.h * 4 bytes.
/// If the copy operation fails, false will be returned.
bool paintengine_get_layer_content(const PaintEngine *dp,
                                   LayerID layer_id,
                                   Rectangle rect,
                                   uint8_t *pixels);

/// Get the number of frames in the layerstack
///
/// When the layerstack is treated as an animation,
/// each non-fixed layer is treated as a frame, therefore
/// the number of frames can be less than the number of layers
/// in the stack.
uintptr_t paintengine_get_frame_count(const PaintEngine *dp);

/// Copy frame pixel data to the given buffer
///
/// This works like paintengine_get_layer_content, with two
/// differences:
///
///  * frame index is used instead of layer ID
///  * background is composited
///  * fixed layers are composited
bool paintengine_get_frame_content(const PaintEngine *dp,
                                   uintptr_t index,
                                   Rectangle rect,
                                   uint8_t *pixels);

const UserACLs *paintengine_get_acl_users(const PaintEngine *dp);

void paintengine_get_acl_layers(const PaintEngine *dp,
                                void *context,
                                void (*visitor)(void *ctx, LayerID id, const LayerACL *layer));

const FeatureTiers *paintengine_get_acl_features(const PaintEngine *dp);

bool paintengine_load_blank(PaintEngine *dp, uint32_t width, uint32_t height, Color background);

bool paintengine_load_file(PaintEngine *dp, const uint16_t *path, uintptr_t path_len);

/// Open a recording for playback
///
/// This clears the existing canvas, just as loading any other file would.
bool paintengine_load_recording(PaintEngine *dp, const uint16_t *path, uintptr_t path_len);

/// Save the currently visible layerstack.
///
/// It is safe to call this function in a separate thread.
bool paintengine_save_file(const PaintEngine *dp, const uint16_t *path, uintptr_t path_len);

bool paintengine_start_recording(PaintEngine *dp, const uint16_t *path, uintptr_t path_len);

void paintengine_stop_recording(PaintEngine *dp);

/// Play back one or more messages from the recording under playback.
///
/// If "sequences" is true, whole undo sequences are stepped instead of
/// single messages. The playback callback will be called at the end.
void paintengine_playback_step(PaintEngine *dp, int32_t steps, bool sequences);

bool paintengine_is_recording(const PaintEngine *dp);

bool paintengine_is_playing(const PaintEngine *dp);

/// Paint all the changed tiles in the given area
///
/// A paintengine instance can only have a single observer (which itself can be
/// a caching layer that is observed by multiple views,) so it keeps track of which
/// tiles have changed since they were last painted. Calling this function will flatten
/// all tiles intersecting the given region of interest and call the provided paint fallback
/// function for each tile with the raw flattened pixel data. The dirty flag is then
/// cleared for the repainted tile.
void paintengine_paint_changes(PaintEngine *dp,
                               void *ctx,
                               Rectangle rect,
                               void (*paint_func)(void *ctx, int32_t x, int32_t y, const uint8_t *pixels));

} // extern "C"

} // namespace rustpile

#endif // DP_RUSTPILE_FFI_H
