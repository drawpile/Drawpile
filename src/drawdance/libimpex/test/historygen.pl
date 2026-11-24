#!/usr/bin/env perl
# SPDX-License-Identifier: GPL-3.0-or-later
use 5.020;
use warnings;
use experimental qw(signatures);
use Encode qw(decode encode);
use File::Basename qw(basename);
use FindBin;


my $last_line = 0;

sub init_test_state ($path) {
    return {
        name => basename($path) =~ s/\..*//rs,
        commands => [],
        local_left => 0,
    };
}

sub push_command ($state, $command) {
    my $type = $command->{type};
    my $index = @{$state->{commands}};
    my $first = $index == 0;
    if ($first && $type ne 'reset') {
        die "first command must be reset\n";
    }

    if ($type eq 'reset') {
        if (!$first) {
            die "reset can only be first command\n";
        }
    }
    elsif ($type eq 'receive') {
        my $count = $command->{count};
        if ($count <= 0) {
            die "receive: invalid count $count\n";
        }

        my $left = $state->{local_left};
        if ($count > $left) {
            die "receive: $count requested, but only $left left\n";
        }

        $state->{local_left} = $left - $count;
    }
    elsif ($type eq 'check') {
        my $left = $state->{local_left};
        if ($left != 0) {
            die "check: still $left local commands left to receive\n";
        }
    }
    elsif ($command->{local}) {
        ++$state->{local_left};
    }

    push @{$state->{commands}}, $command;
}

sub finish_state ($state) {
    if (!@{$state->{commands}}) {
        die "No commands in test\n";
    }

    my $left = $state->{local_left};
    if ($left != 0) {
        die "Still $left local commands left at end of test\n";
    }
}

sub parse_test ($path, $lineno) {
    open my $fh, '<', $path or die "Can't open '$path': $!\n";

    my $state = init_test_state($path);

    while (<$fh>) {
        ++$$lineno;
        my $line = decode('UTF-8', $_);
        next unless $line =~ /\S/;
        next if $line =~ /\A\s*#/;

        $line =~ s/\A\s+|\s+\z//gs;
        $line =~ s/\s+/ /gs;

        if ($line =~ /\Areset ([0-9]+)x([0-9]+) ([0-9]+)\z/) {
            push_command($state, {
                type => 'reset',
                line => $line,
                width => 0 + $1,
                height => 0 + $2,
                undo_limit => 0 + $3,
            });
        }
        elsif ($line =~ /\Areceive ([0-9]+)\z/) {
            push_command($state, {
                type => 'receive',
                line => $line,
                count => 0 + $1,
            });
        }
        elsif ($line =~ /\Acheck\z/) {
            push_command($state, {
                type => 'check',
                line => $line,
            });
        }
        elsif ($line =~ /\A(local|remote) resize (-?[0-9]+),(-?[0-9]+),(-?[0-9]+),(-?[0-9]+)\z/) {
            push_command($state, {
                type => 'resize',
                line => $line,
                local => $1 eq 'local',
                top => 0 + $2,
                right => 0 + $3,
                bottom => 0 + $4,
                left => 0 + $5,
            });
        }
        elsif ($line =~ /\A(local|remote) fill ([0-9]+),([0-9]+) ([0-9]+)x([0-9]+) ([0-9a-f]{6})\z/) {
            push_command($state, {
                type => 'fill',
                line => $line,
                local => $1 eq 'local',
                x => 0 + $2,
                y => 0 + $3,
                width => 0 + $4,
                height => 0 + $5,
                color => $6,
            });
        }
        elsif ($line =~ /\A(local|remote) undo\z/) {
            push_command($state, {
                type => 'undo',
                line => $line,
                local => $1 eq 'local',
            });
        }
        elsif ($line =~ /\A(local|remote) redo\z/) {
            push_command($state, {
                type => 'redo',
                line => $line,
                local => $1 eq 'local',
            });
        }
        else {
            die "Can't parse line\n";
        }
    }

    close $fh or die "Can't close '$path': $!\n";
    finish_state($state);
    return $state;
}

sub parse_tests (@paths) {
    my @states;
    for my $path (@paths) {
        my $lineno = 0;
        if (!eval { push @states, parse_test($path, \$lineno); 1 }) {
            my $err = $@ =~ s/\s+\z//r;
            die "$err\n\tin $path\n\tline $lineno\n";
        }
    }
    return @states;
}

sub generate_test ($fh, $state) {
    print {$fh} <<~END_OF_FUNCTION_HEADER;

    static void history_$state->{name}(TEST_PARAMS)
    {
        DP_TestHistoryContext *thc = thc_new(__func__);

    END_OF_FUNCTION_HEADER

    for my $command (@{$state->{commands}}) {
        print {$fh} "    // $command->{line}\n";

        my $type = $command->{type};
        if ($type eq 'reset') {
            print {$fh} <<~END_OF_RESET;
                handle_reset(thc, $command->{width}, $command->{height}, $command->{undo_limit});
            END_OF_RESET
        }
        elsif ($type eq 'receive') {
            print {$fh} <<~END_OF_RESET;
                handle_receive(thc, $command->{count});
            END_OF_RESET
        }
        elsif ($type eq 'check') {
            print {$fh} <<~END_OF_RESET;
                handle_check(TEST_ARGS, thc);
            END_OF_RESET
        }
        elsif ($type eq 'resize') {
            my $is_local = $command->{local} ? 'true' : 'false';
            print {$fh} <<~END_OF_RESET;
                handle_resize(thc, $is_local, $command->{top}, $command->{right}, $command->{bottom}, $command->{left});
            END_OF_RESET
        }
        elsif ($type eq 'fill') {
            my $is_local = $command->{local} ? 'true' : 'false';
            print {$fh} <<~END_OF_RESET;
                handle_fill(thc, $is_local, $command->{x}, $command->{y}, $command->{width}, $command->{height}, 0xff$command->{color}u);
            END_OF_RESET
        }
        elsif ($type eq 'undo') {
            my $is_local = $command->{local} ? 'true' : 'false';
            print {$fh} <<~END_OF_RESET;
                handle_undo_redo(thc, $is_local, false);
            END_OF_RESET
        }
        elsif ($type eq 'redo') {
            my $is_local = $command->{local} ? 'true' : 'false';
            print {$fh} <<~END_OF_RESET;
                handle_undo_redo(thc, $is_local, true);
            END_OF_RESET
        }
        else {
            die "Unhandled type $type\n";
        }
    }

    print {$fh} <<~END_OF_FUNCTION_FOOTER;

        handle_check(TEST_ARGS, thc);
        thc_free(thc);
    }

    END_OF_FUNCTION_FOOTER
}

sub generate_tests ($path, @states) {
    open my $fh, '>', $path or die "Can't open '$path': $!\n";

    print {$fh} <<~END_OF_HEADER;
    // SPDX-License-Identifier: GPL-3.0-or-later
    // This file was auto-generated by historygen.pl. Don't edit it manually.
    #include <dpcommon/common.h>
    #include <dpcommon/conversions.h>
    #include <dpcommon/output.h>
    #include <dpcommon/queue.h>
    #include <dpengine/canvas_history.h>
    #include <dpengine/canvas_state.h>
    #include <dpengine/draw_context.h>
    #include <dpengine/layer_content.h>
    #include <dpengine/layer_list.h>
    #include <dpengine/layer_props.h>
    #include <dpengine/layer_props_list.h>
    #include <dpengine/tile.h>
    #include <dpimpex/save.h>
    #include <dpmsg/blend_mode.h>
    #include <dpmsg/ids.h>
    #include <dpmsg/message.h>
    #include <dptest_impex.h>

    #define LOCAL_USER        1u
    #define REMOTE_USER       2u
    #define LAYER_ID          DP_layer_id_make(1u, 0)
    #define PROTOCOL_LAYER_ID DP_layer_id_to_protocol(LAYER_ID)

    typedef struct DP_TestHistoryMessages {
        int count;
        DP_Message *msgs[];
    } DP_TestHistoryMessages;

    typedef struct DP_TestHistoryContext {
        char *title;
        DP_DrawContext *dc;
        DP_CanvasHistory *ch_local;
        DP_CanvasHistory *ch_remote;
        DP_Queue pending_msgs;
        int checks;
    } DP_TestHistoryContext;

    static DP_TestHistoryMessages *msgs_new(int count)
    {
        DP_TestHistoryMessages *thm = DP_malloc(
            DP_FLEX_SIZEOF(DP_TestHistoryMessages, msgs, DP_int_to_size(count)));
        thm->count = count;
        for (int i = 0; i < count; ++i) {
            thm->msgs[i] = NULL;
        }
        return thm;
    }

    static void msgs_free(DP_TestHistoryMessages *thm)
    {
        if (thm) {
            int count = thm->count;
            for (int i = 0; i < count; ++i) {
                DP_message_decref_nullable(thm->msgs[i]);
            }
            DP_free(thm);
        }
    }

    static DP_TestHistoryContext *thc_new(const char *title)
    {
        DP_TestHistoryContext *thc = DP_malloc(sizeof(*thc));
        thc->title = DP_strdup(title);
        thc->dc = DP_draw_context_new();
        thc->ch_local = DP_canvas_history_new_no_mutex();
        thc->ch_remote = DP_canvas_history_new_no_mutex();
        DP_queue_init(&thc->pending_msgs, sizeof(DP_TestHistoryMessages *), 8);
        thc->checks = 0;
        return thc;
    }

    static void thc_free(DP_TestHistoryContext *thc)
    {
        if (thc) {
            DP_queue_dispose(&thc->pending_msgs);
            DP_canvas_history_free(thc->ch_remote);
            DP_canvas_history_free(thc->ch_local);
            DP_draw_context_free(thc->dc);
            DP_free(thc->title);
            DP_free(thc);
        }
    }

    static unsigned int context_id_for(bool is_local)
    {
        if (is_local) {
            return LOCAL_USER;
        }
        else {
            return REMOTE_USER;
        }
    }

    static void process_msgs(DP_TestHistoryContext *thc,
                             DP_TestHistoryMessages *thm, bool is_local)
    {
        int count = thm->count;
        if (is_local) {
            for (int i = 0; i < count; ++i) {
                DP_canvas_history_handle_local(thc->ch_local, thc->dc,
                                               thm->msgs[i]);
            }
            *(DP_TestHistoryMessages **)DP_queue_push(&thc->pending_msgs, sizeof(DP_TestHistoryMessages *)) = thm;
        }
        else {
            for (int i = 0; i < count; ++i) {
                DP_canvas_history_handle(thc->ch_remote, thc->dc, thm->msgs[i]);
                DP_canvas_history_handle(thc->ch_local, thc->dc, thm->msgs[i]);
            }
            msgs_free(thm);
        }
    }

    static DP_CanvasState *make_initial_canvas_state(DP_DrawContext *dc, int width,
                                                     int height)
    {
        DP_TransientCanvasState *tcs = DP_transient_canvas_state_new_init();
        DP_transient_canvas_state_width_set(tcs, width);
        DP_transient_canvas_state_height_set(tcs, height);
        DP_transient_canvas_state_background_tile_set_noinc(
            tcs, DP_tile_new_from_bgra(0u, 0xffffffffu), true);

        DP_TransientLayerContent *tlc =
            DP_transient_layer_content_new_init(width, height, NULL);
        DP_transient_layer_list_insert_transient_content_noinc(
            DP_transient_canvas_state_transient_layers(tcs, 1), tlc, 0);

        DP_TransientLayerProps *tlp =
            DP_transient_layer_props_new_init(LAYER_ID, false);
        DP_transient_layer_props_list_insert_transient_noinc(
            DP_transient_canvas_state_transient_layer_props(tcs, 1), tlp, 0);

        DP_transient_canvas_state_layer_routes_reindex(tcs, dc);
        return DP_transient_canvas_state_persist(tcs);
    }

    static void handle_reset(DP_TestHistoryContext *thc, int width, int height,
                             int undo_depth_limit)
    {
        DP_CanvasState *cs = make_initial_canvas_state(thc->dc, width, height);
        DP_canvas_state_incref(cs);
        DP_canvas_history_reset_to_state_noinc(thc->ch_local, cs);
        DP_canvas_history_reset_to_state_noinc(thc->ch_remote, cs);
        DP_canvas_history_undo_depth_limit_set(thc->ch_local, thc->dc,
                                               undo_depth_limit);
        DP_canvas_history_undo_depth_limit_set(thc->ch_remote, thc->dc,
                                               undo_depth_limit);
    }

    static void handle_resize(DP_TestHistoryContext *thc, bool is_local,
                              int32_t top, int32_t right, int32_t bottom,
                              int32_t left)
    {
        unsigned int context_id = context_id_for(is_local);
        DP_TestHistoryMessages *thm = msgs_new(2);
        thm->msgs[0] = DP_msg_undo_point_new(context_id);
        thm->msgs[1] =
            DP_msg_canvas_resize_new(context_id, top, right, bottom, left);
        process_msgs(thc, thm, is_local);
    }

    static void handle_fill(DP_TestHistoryContext *thc, bool is_local, uint32_t x,
                            uint32_t y, uint32_t w, uint32_t h, uint32_t color)
    {
        unsigned int context_id = context_id_for(is_local);
        DP_TestHistoryMessages *thm = msgs_new(2);
        thm->msgs[0] = DP_msg_undo_point_new(context_id);
        thm->msgs[1] = DP_msg_fill_rect_new(
            context_id, PROTOCOL_LAYER_ID, DP_BLEND_MODE_NORMAL, x, y, w, h, color);
        process_msgs(thc, thm, is_local);
    }

    static void handle_undo_redo(DP_TestHistoryContext *thc, bool is_local,
                                 bool redo)
    {
        unsigned int context_id = context_id_for(is_local);
        DP_TestHistoryMessages *thm = msgs_new(1);
        thm->msgs[0] = DP_msg_undo_new(context_id, 0, redo);
        process_msgs(thc, thm, is_local);
    }

    static void handle_receive(DP_TestHistoryContext *thc, int receive_count)
    {
        int pending = DP_size_to_int(thc->pending_msgs.used);
        if (receive_count > pending) {
            DP_panic("Trying to receive %d messages, but only %d pending",
                     receive_count, pending);
        }

        for (int i = 0; i < receive_count; ++i) {
            DP_TestHistoryMessages *thm =
                *(DP_TestHistoryMessages **)DP_queue_peek(&thc->pending_msgs, sizeof(DP_TestHistoryMessages *));

            int msg_count = thm->count;
            for (int j = 0; j < msg_count; ++j) {
                DP_canvas_history_handle(thc->ch_remote, thc->dc, thm->msgs[j]);
                DP_canvas_history_handle(thc->ch_local, thc->dc, thm->msgs[j]);
            }

            msgs_free(thm);
            DP_queue_shift(&thc->pending_msgs);
        }
    }

    static void write_png(const char *path, DP_CanvasHistory *ch,
                          DP_DrawContext *dc)
    {
        DP_CanvasState *cs = DP_canvas_history_get_noinc_nolock(ch);
        DP_SaveResult result =
            DP_save(cs, dc, DP_SAVE_IMAGE_PNG, path, NULL, NULL, NULL);
        if (result != DP_SAVE_RESULT_SUCCESS) {
            DP_panic("Error saving to %s: %s", path, DP_error());
        }
    }

    static void handle_check(TEST_PARAMS, DP_TestHistoryContext *thc)
    {
        int check = ++thc->checks;
        const char *title = thc->title;
        size_t pending = thc->pending_msgs.used;
        if (pending != (size_t)0) {
            DP_panic("%s check %d: %zu messages still pending", title, check, pending);
        }

        int local_fork_count = DP_canvas_history_local_fork_count(thc->ch_local);
        if (local_fork_count != 0) {
            DP_panic("%s check %d: local fork present with %d messages", title, check, local_fork_count);
        }

        int remote_fork_count = DP_canvas_history_local_fork_count(thc->ch_local);
        if (remote_fork_count != 0) {
            DP_panic("%s check %d: remote fork present with %d messages", title, check, remote_fork_count);
        }

        char *local_path =
            DP_format("test/tmp/history_local_%s%04d.png", title, check);
        char *remote_path =
            DP_format("test/tmp/history_remote_%s%04d.png", title, check);

        write_png(local_path, thc->ch_local, thc->dc);
        write_png(remote_path, thc->ch_remote, thc->dc);

        IMAGE_FILE_EQ_OK(local_path, remote_path, "%s check %d: images match", title, check);

        DP_free(remote_path);
        DP_free(local_path);
    }
    END_OF_HEADER

    for my $state (@states) {
        generate_test($fh, $state);
    }

    print {$fh} <<~END_OF_LAST_TEST;

    enum DP_TestHistoryType {
        DP_TEST_HISTORY_TYPE_FILL,
        DP_TEST_HISTORY_TYPE_RESIZE,
        DP_TEST_HISTORY_TYPE_UNDO_REDO,
        DP_TEST_HISTORY_TYPE_RECEIVE,
        DP_TEST_HISTORY_TYPE_COUNT,
    };

    static bool random_bool(void)
    {
        return rand() % 2 != 0;
    }

    static int random_int_between(int min_inclusive, int max_inclusive)
    {
        if (min_inclusive < max_inclusive) {
            int n = max_inclusive - min_inclusive + 1;
            return min_inclusive + (rand() % n);
        }
        else {
            return min_inclusive;
        }
    }

    static uint32_t random_color(bool is_local)
    {
        uint32_t c = DP_int_to_uint32(rand() % 128);
        c = (uint32_t)0xff000000u | (c << (uint32_t)16u) | (c << (uint32_t)8u) | c;
        if (is_local) {
            return c | (uint32_t)0xff0000u;
        }
        else {
            return c | (uint32_t)0xff;
        }
    }

    static bool dimension_in_bounds(int dimension)
    {
        // Prevent canvas dimensions getting too wacky.
        return dimension >= 100 && dimension <= 2000;
    }

    static int get_random_steps_from_environment(void)
    {
        const char *value = getenv("DP_TEST_RANDOM_HISTORY_STEPS");
        if (value) {
            int steps = atoi(value);
            if (steps > 0) {
                return steps;
            }
        }
        return random_int_between(10, 100);
    }

    static void random_history(TEST_PARAMS)
    {
        const char *path = "test/tmp/history_random.test";
        DP_Output *out = DP_file_output_new_from_path(path);
        if (!NOT_NULL_OK(out, "open random test output")) {
            DIAG("Error opening %s: %s", path, DP_error());
            return;
        }

        OK(DP_output_format(out, "# Random test with seed %u\\n", TEST_SRAND_SEED),
           "write header");

        DP_TestHistoryContext *thc = thc_new(__func__);

        int width = random_int_between(500, 1000);
        int height = random_int_between(500, 1000);
        int undo_depth_limit = random_int_between(3, 255);
        handle_reset(thc, width, height, undo_depth_limit);
        OK(DP_output_format(out, "reset %dx%d %d\\n", width, height,
                            undo_depth_limit),
           "write reset");

        int steps = get_random_steps_from_environment();
        for (int i = 0; i < steps; ++i) {
            int type = random_int_between(0, DP_TEST_HISTORY_TYPE_COUNT - 1);
            switch (type) {
            case DP_TEST_HISTORY_TYPE_FILL: {
                bool is_local = random_bool();
                int x = random_int_between(0, width - 1);
                int y = random_int_between(0, height - 1);
                int w = random_int_between(1, width - x);
                int h = random_int_between(1, height - y);
                uint32_t color = random_color(is_local);
                handle_fill(thc, is_local, DP_int_to_uint32(x), DP_int_to_uint32(y),
                            DP_int_to_uint32(w), DP_int_to_uint32(h), color);
                OK(DP_output_format(out, "%s fill %d,%d %dx%d %06x\\n",
                                    is_local ? "local" : "remote", x, y, w, h,
                                    color & (uint32_t)0xffffffu),
                   "write %d fill", i);
                break;
            }
            case DP_TEST_HISTORY_TYPE_RESIZE: {
                bool is_local = random_bool();
                int top, right, bottom, left;
                do {
                    top = random_int_between(-1, 1);
                    right = random_int_between(-1, 1);
                    bottom = random_int_between(-1, 1);
                    left = random_int_between(-1, 1);
                } while ((top == 0 && right == 0 && bottom == 0 && left == 0)
                         || !dimension_in_bounds(width + left + right)
                         || !dimension_in_bounds(height + top + bottom));
                handle_resize(thc, is_local, DP_int_to_int32(top),
                              DP_int_to_int32(right), DP_int_to_int32(bottom),
                              DP_int_to_int32(left));
                OK(DP_output_format(out, "%s resize %d,%d,%d,%d\\n",
                                    is_local ? "local" : "remote", top, right,
                                    bottom, left),
                   "write %d resize", i);
                width += left + right;
                height += top + bottom;
                break;
            }
            case DP_TEST_HISTORY_TYPE_UNDO_REDO: {
                bool is_local = random_bool();
                bool redo = random_int_between(0, 3) == 0;
                handle_undo_redo(thc, is_local, redo);
                OK(DP_output_format(out, "%s %s\\n", is_local ? "local" : "remote",
                                    redo ? "redo" : "undo"),
                   "write %d undo/redo", i);
                break;
            }
            case DP_TEST_HISTORY_TYPE_RECEIVE: {
                int pending = DP_size_to_int(thc->pending_msgs.used);
                int count = DP_min_int(pending, random_int_between(0, pending * 2));
                if (count > 0) {
                    handle_receive(thc, count);
                    OK(DP_output_format(out, "receive %d\\n", count),
                       "write %d receive", i);
                    if (thc->pending_msgs.used == 0) {
                        handle_check(TEST_ARGS, thc);
                        OK(DP_OUTPUT_PRINT_LITERAL(out, "check\\n"),
                           "write %d check", i);
                    }
                }
                break;
            }
            }
        }

        int final_pending = DP_size_to_int(thc->pending_msgs.used);
        if (final_pending > 0) {
            handle_receive(thc, final_pending);
            OK(DP_output_format(out, "receive %d\\n", final_pending),
               "write final receive");
        }

        handle_check(TEST_ARGS, thc);
        thc_free(thc);

        OK(DP_output_free(out), "free output");
    }

    static void register_tests(REGISTER_PARAMS)
    {
    END_OF_LAST_TEST

    for my $state (@states) {
        print {$fh} "    REGISTER_TEST(history_$state->{name});\n";
    }

    print {$fh} <<~END_OF_FOOTER;
        REGISTER_TEST(random_history);
    }

    int main(int argc, char **argv)
    {
        DP_test_main(argc, argv, register_tests, NULL);
    }
    END_OF_FOOTER

    close $fh or die "Can't close '$fh': $!\n";

    system 'clang-format', '-i', $path;
}

generate_tests(
    "$FindBin::Bin/history.c",
    parse_tests(glob "$FindBin::Bin/history/*.test"));
