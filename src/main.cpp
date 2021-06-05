#include <iostream>
#include <cstdio>
#include <cfloat>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include <getopt.h>     /* getopt_long() */

#include "memory_mlx90640.hpp"
#include "mlx90640.hpp"
#include "dev_handler.hpp"
#include "push_data.hpp"

static const char short_options[] = "d:n:hmrf:S:R:CXt:x:";

static const struct option
long_options[] = {
    { "device",     required_argument,  NULL, 'd' },
    { "nvram",      required_argument,  NULL, 'n' },
    { "help",       no_argument,        NULL, 'h' },
    { "mmap",       no_argument,        NULL, 'm' },
    { "read",       no_argument,        NULL, 'r' },
    { "fps",        required_argument,  NULL, 'f' },
    { "save",       required_argument,  NULL, 'S' },
    { "save-raw",   required_argument,  NULL, 'R' },
    { "ignore-EE-check",  no_argument,  NULL, 'C' },
    { "extended-format",  no_argument,  NULL, 'X' },
    { "interp-type", required_argument, NULL, 't' },
    { "interp-ratio", required_argument, NULL, 'x' },
    { 0, 0, 0, 0 }
};

static void usage(FILE *fp, int, char**argv)
{
    fprintf(fp,
            "Usage: %s [options]\n\n"
            "Options:\n"
            "[Generic]\n"
            "-h | --help                Print this message\n"
            "-R | --save-raw PATH       Save raw data from the device to PATH\n"
            "-S | --save PATH           Save raw video feed to PATH\n"
            "               (Post-processed, Min-max mapped, gray16-le)\n"
            "[Data Source]\n"
            "-d | --device PATH         [REQUIRED] Video device file path\n"
            "               If the file appear to be not a device file,\n"
            "               then the program will fall back to raw file read.\n"
            "-n | --nvram PATH          [REQUIRED] NVRAM file path\n"
            "-C | --ignore-EE-check     Skip NVRAM validity check\n"
            "-f | --fps                 Set feed update frequency [default: 4FPS]\n"
            "               If device is a raw file and fps is not given or -1,\n"
            "               then the file will be processed as fast as possible.\n"
            "V4L2 only:\n"
            "-m | --mmap                Use memory mapped buffers [default]\n"
            "-r | --read                Use read() calls\n"
            "Raw file read only:\n"
            "-X | --extended-format     Treat the file as 27 lines per frame\n"
            "[GStreamer videoscale options]\n"
            "Note: This program does not relay over GStreamer arguments. However,\n"
            "      environement variables still apply.\n"
            "In the pipeline, there is a videoscale element after appsrc. This element is\n"
            "intended to apply a fancy interpolation, because nearest-neighbor or bilinear\n"
            "method up to full scale will produce ugly result.\n"
            "After this \"firsthand\" scaling, gstopengl will apply another scaling to fit\n"
            "the output frame.\n"
            "-t | --interp-type         Interpolation type of the scaling, int [default: 7]\n"
            "               Refer to Gstreamer videoscale plugin documentation\n"
            "-x | --interp-ratio        Scale factor of firsthand scaling [default: 7]\n"
            "               The width and height both will be multiplied with this factor\n"
            "",
            argv[0]);
}

int main(int argc, char **argv) {
    mlx90640 mlx = mlx90640();
    dev_handler* device;

    char * dev_name = NULL;
    char * nv_name = NULL;

    char * fps_ = NULL;
    int fps = -1;

    bool save = false;
    char * save_path = NULL;
    bool save_raw = false;
    char * save_raw_path = NULL;

    int io_method = dev_handler::IO_METHOD_MMAP;
    bool ignore_ee_check = false;
    bool extended_format = false;

    int interp_type = 7;
    int interp_ratio = 7;

    char overlay_buf[64];

    for (;;) {
        int idx;
        int c;

        c = getopt_long(argc, argv,
                        short_options, long_options, &idx);

        if (c == -1)
            break;

        switch (c) {
        case 0: /* getopt_long() flag */
            break;

        case 'd':
            dev_name = optarg;
            break;

        case 'n':
            nv_name = optarg;
            break;

        case 'h':
            usage(stdout, argc, argv);
            exit(EXIT_SUCCESS);
            break;

        case 'm':
            io_method = dev_handler::IO_METHOD_MMAP;
            break;

        case 'r':
            io_method = dev_handler::IO_METHOD_READ;
            break;

        case 'f':
            fps_ = optarg;
            fps = (int)std::stof(fps_);
            break;

        case 'S':
            save = true;
            save_path = optarg;
            break;

        case 'R':
            save_raw = true;
            save_raw_path = optarg;
            break;

        case 'C':
            ignore_ee_check = true;
            break;

        case 'X':
            extended_format = true;
            break;

        case 't':
            interp_type = std::stoi(optarg);
            break;

        case 'x':
            interp_ratio = std::stoi(optarg);
            break;

        default:
            fprintf(stderr, "Unrecognized option: %c\n", c);
            usage(stdout, argc, argv);
            exit(EXIT_FAILURE);
            break;
        }
    }

    if (dev_name == NULL || nv_name == NULL) {
        printf("Required option not given\n");
        usage(stdout, argc, argv);
        exit(EXIT_FAILURE);
    }

    device = new dev_handler(io_method, fps, extended_format);
    device->init_frame_file(dev_name);

    if (!mlx.init_ee(nv_name, ignore_ee_check)) {
        printf("NVMEM initialization error\n");
        exit(EXIT_FAILURE);
    }

    if (gst_init_(interp_type, interp_ratio) != 0) {
        printf("Gstreamer initialization error\n");
        exit(EXIT_FAILURE);
    }

    mlx.init_frame_file(device);
    gst_start_running();

    uint16_t To_int[0x300];
    uint8_t * dest;
    const mlx90640::pixel *pixels;
    FILE* save_LE16_frm;
    FILE* save_pixel_raw;
    if (save)
        save_LE16_frm = fopen(save_path, "wb");
    if (save_raw)
        save_pixel_raw = fopen(save_raw_path, "wb");

    while (1) {
        if (!mlx.process_frame_file()) {
            printf("Stopping due to file read\n");
            break;
        }

        dest = gst_get_userp();
        if (dest == NULL) {
            printf("Stopping due to gstreamer frame init\n");
            break;
        }

        mlx.process_frame();
        mlx.process_pixel();

        pixels = mlx.pix_notable();
        // mapping: a(x-b) = range * (x-min) / (max - min)
        double b = pixels[mlx90640::MIN_T].T;
        double a = 65535.0 / (pixels[mlx90640::MAX_T].T - pixels[mlx90640::MIN_T].T);

        for (int row = 0; row < 24; row++) {
            for (int col = 0; col < 32; col++) {
                double result = a * ((mlx.To_())[row * 32 + col] - b);
                if (result >= 65536) {  // somehow conversion yielded values bigger than 65535 + DBL_EPSILON, but by miniscule value
                                        // Int conversion will always round down.
                    printf("WARNING: mapping result too big\n");
                    printf("min: %lf, max: %lf, To: %lf, Result: %lf\n",
                        pixels[mlx90640::MIN_T].T,
                        pixels[mlx90640::MAX_T].T,
                        (mlx.To_())[row * 32 + col],
                        result);
                    result = 65535;
                }
                if (result < 0) {
                    printf("WARNING: mapping result negative\n");
                    printf("min: %lf, max: %lf, To: %lf, Result: %lf\n",
                        pixels[mlx90640::MIN_T].T,
                        pixels[mlx90640::MAX_T].T,
                        (mlx.To_())[row * 32 + col],
                        result);
                    result = 0;
                }
                To_int[row * 32 + col] = result;
            }
        }
        if (save)
            fwrite(To_int, sizeof(uint16_t), 0x300, save_LE16_frm);
        if (save_raw)
            fwrite(mlx.Pix_Raw_(), sizeof(uint16_t), device->is_extended() ? 0x360 : 0x340, save_pixel_raw);


        memcpy(dest, To_int, 0x600);

        snprintf(overlay_buf, 64, "MAX: %.2lf\nMIN: %.2lf\nMID: %.2lf",
            pixels[mlx90640::MAX_T].T,
            pixels[mlx90640::MIN_T].T,
            pixels[mlx90640::SCENE_CENTER].T);

        if (!gst_arm_buffer(overlay_buf)) {
            printf("Stopping due to Gstreamer frame processing\n");
            break;
        }
    }

    printf("closing\n");
    if (save) {
        fclose(save_LE16_frm);
    }
    if (save_raw) {
        fclose(save_pixel_raw);
    }

    gst_cleanup();

    delete device;
    return 0;
}
