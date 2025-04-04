#!/bin/bash
if [[ $# -ne 4 ]]; then
    echo "Usage: $0 BENCH_MULTIDAB_EXECUTABLE TOTAL_DABS DABS_PER_MESSAGE OUTPUT_DIR" 1>&2
    exit 2
fi

bench_multidab="$1"
total_dabs="$2"
dabs_per_message="$3"
output_dir="$4"
max_size=255

blend_modes=(
    'svg:dst-out'
    'svg:src-over'
    'svg:multiply'
    'krita:divide'
    'svg:color-burn'
    'svg:color-dodge'
    'svg:darken'
    'svg:lighten'
    'krita:subtract'
    'svg:plus'
    'svg:src-atop'
    'svg:dst-over'
    '-dp-cerase'
    'svg:screen'
    '-dp-normal-and-eraser'
    'krita:luminosity_sai'
    'svg:overlay'
    'svg:hard-light'
    'svg:soft-light'
    '-dp-linear-light'
    'svg:hue'
    'svg:saturation'
    'svg:luminosity'
    'svg:color'
    '-dp-replace'
)

modes_header='Indirect,Erase,Normal,Multiply,Divide,Burn,Dodge,Darken,Lighten,Subtract,Add,Recolor,Behind,Color Erase,Screen,Normal and Eraser,Luminosity/Shine (SAI),Overlay,Hard Light,Soft Light,Linear Light,Hue,Saturation,Luminosity,Color,Replace'

run_bench() {
    printf '%s' "$($bench_multidab "$@")"
}

bench_pixel() {
    local pixel_type
    pixel_type="pixel$1"
    printf '%s\n' "$modes_header"
    for ((size = 0 ; size <= "$max_size" ; size++)); do
        echo "bench $pixel_type $size/$max_size" 1>&2
        run_bench "$pixel_type" "$total_dabs" "$dabs_per_message" 255 svg:src-over "$size" 255
        for blend_mode in "${blend_modes[@]}"; do
            printf ','
            run_bench "$pixel_type" "$total_dabs" "$dabs_per_message" 0 "$blend_mode" "$size" 255
        done
        printf '\n'
    done
}

bench_classic() {
    printf '%s\n' "$modes_header"
    for ((size = 0 ; size <= "$max_size" ; size++)); do
        echo "bench classic $size/$max_size" 1>&2
        ((smooth_size = size * 257))
        run_bench classic "$total_dabs" "$dabs_per_message" 255 svg:src-over "$smooth_size" 255 127
        for blend_mode in "${blend_modes[@]}"; do
            printf ','
            run_bench classic "$total_dabs" "$dabs_per_message" 0 "$blend_mode" "$smooth_size" 255 127
        done
        printf '\n'
    done
}

bench_mypaint() {
    printf 'Indirect,Normal,Lock Alpha,Colorize,Posterize,N+LA,N+LA+C,N+LA+C+P\n'
    for ((size = 0 ; size <= "$max_size" ; size++)); do
        echo "bench mypaint $size/$max_size" 1>&2
        ((smooth_size = size * 257))
        run_bench mypaint "$total_dabs" "$dabs_per_message" 255 "$smooth_size" 255 127 0 0 0 0 0 129
        printf ','
        run_bench mypaint "$total_dabs" "$dabs_per_message" 255 "$smooth_size" 255 127 0 0 0 0 0 0
        printf ','
        run_bench mypaint "$total_dabs" "$dabs_per_message" 255 "$smooth_size" 255 127 0 0 255 0 0 0
        printf ','
        run_bench mypaint "$total_dabs" "$dabs_per_message" 255 "$smooth_size" 255 127 0 0 0 255 0 0
        printf ','
        run_bench mypaint "$total_dabs" "$dabs_per_message" 255 "$smooth_size" 255 127 0 0 0 0 255 127
        printf ','
        run_bench mypaint "$total_dabs" "$dabs_per_message" 255 "$smooth_size" 255 127 0 0 127 0 0 0
        printf ','
        run_bench mypaint "$total_dabs" "$dabs_per_message" 255 "$smooth_size" 255 127 0 0 85 85 0 0
        printf ','
        run_bench mypaint "$total_dabs" "$dabs_per_message" 255 "$smooth_size" 255 127 0 0 64 64 64 127
        printf '\n'
    done
}

bench_pixel round >"$output_dir/pixelround-$total_dabs-$dabs_per_message.csv"
bench_pixel square >"$output_dir/pixelsquare-$total_dabs-$dabs_per_message.csv"
bench_classic >"$output_dir/classic-$total_dabs-$dabs_per_message.csv"
bench_mypaint >"$output_dir/mypaint-$total_dabs-$dabs_per_message.csv"
