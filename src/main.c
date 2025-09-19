/*
Copyright 2025 Amini Allight

This file is part of wlgamma.

wlgamma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

wlgamma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with wlgamma. If not, see <https://www.gnu.org/licenses/>.
*/
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wayland-client.h>
#include "wlr-gamma-control.h"

static const char* const program_name = "wlgamma";
static const int major_version = 0;
static const int minor_version = 1;
static const int patch_version = 0;
static const char* const shm_path = "/wlgamma";
static const int channel_count = 3;

static struct wl_display* display = NULL;
static struct wl_registry* registry = NULL;
static int output_count = 0;
static struct wl_output** outputs = NULL;
static char** output_names = NULL;
static struct zwlr_gamma_control_manager_v1* gamma_control_manager = NULL;
static struct zwlr_gamma_control_v1* gamma_control = NULL;
static int quit = 0;
static uint32_t gamma_size = 0;
static int shm_fd = -1;
static uint16_t* shm_data = NULL;

void on_output_geometry(
    void* userdata,
    struct wl_output* output,
    int32_t x,
    int32_t y,
    int32_t physical_width,
    int32_t physical_height,
    int32_t subpixel,
    const char* make,
    const char* model,
    int32_t transform
)
{
    /* Do nothing */
}

void on_output_mode(
    void* userdata,
    struct wl_output* output,
    uint32_t flags,
    int32_t width,
    int32_t height,
    int32_t refresh
)
{
    /* Do nothing */
}

void on_output_done(
    void* userdata,
    struct wl_output* output
)
{
    /* Do nothing */
}

void on_output_scale(
    void* userdata,
    struct wl_output* output,
    int32_t factor
)
{
    /* Do nothing */
}

void on_output_name(
    void* userdata,
    struct wl_output* output,
    const char* name
)
{
    for (int i = 0; i < output_count; i++)
    {
        if (outputs[i] == output)
        {
            output_names[i] = strdup(name);
        }
    }
}

void on_output_description(
    void* userdata,
    struct wl_output* output,
    const char* description
)
{
    /* Do nothing */
}

static const struct wl_output_listener output_listener = {
    .geometry = on_output_geometry,
    .mode = on_output_mode,
    .done = on_output_done,
    .scale = on_output_scale,
    .name = on_output_name,
    .description = on_output_description
};

static void on_registry_global(
    void* userdata,
    struct wl_registry* registry,
    uint32_t id,
    const char* interface,
    uint32_t version
)
{
    if (strcmp(interface, zwlr_gamma_control_manager_v1_interface.name) == 0)
    {
        gamma_control_manager = (struct zwlr_gamma_control_manager_v1*)wl_registry_bind(registry, id, &zwlr_gamma_control_manager_v1_interface, 1);
    }
    else if (strcmp(interface, wl_output_interface.name) == 0)
    {
        struct wl_output* output = (struct wl_output*)wl_registry_bind(registry, id, &wl_output_interface, 1);
        wl_output_add_listener(output, &output_listener, NULL);

        int new_output_count = output_count + 1;
        struct wl_output** new_outputs = malloc(new_output_count * sizeof(struct wl_output*));
        char** new_output_names = malloc(new_output_count * sizeof(char*));

        memcpy(new_outputs, outputs, output_count * sizeof(struct wl_output*));
        memcpy(new_output_names, output_names, output_count * sizeof(char*));
        new_outputs[output_count] = output;
        new_output_names[output_count] = NULL;

        free(outputs);
        free(output_names);
        outputs = new_outputs;
        output_names = new_output_names;
        output_count = new_output_count;
    }
}

static void on_registry_global_remove(
    void* userdata,
    struct wl_registry* registry,
    uint32_t id
)
{
    /* Do nothing */
}

static void on_gamma_control_gamma_size(
    void* userdata,
    struct zwlr_gamma_control_v1* gamma_control,
    uint32_t size
)
{
    gamma_size = size;
}

static void on_gamma_control_failed(
    void* userdata,
    struct zwlr_gamma_control_v1* gamma_control
)
{
    fprintf(stderr, "Gamma control failure reported by Wayland server.\n");
}

static const struct wl_registry_listener registry_listener = {
    .global = on_registry_global,
    .global_remove = on_registry_global_remove
};

static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
    .gamma_size = on_gamma_control_gamma_size,
    .failed = on_gamma_control_failed
};

static void on_interrupt(int sig)
{
    quit = 1;
}

static uint32_t gamma_table_size()
{
    return channel_count * sizeof(uint16_t) * gamma_size;
}

static void fill_gamma_ramp(uint16_t* ramp, float component, float gamma)
{
    for (uint32_t i = 0; i < gamma_size; i++)
    {
        ramp[i] = UINT16_MAX * powf((i / (float)(gamma_size - 1)) * component, 1.0f / gamma);
    }
}

static void fill_gamma_table(float* components, float gamma)
{
    for (int i = 0; i < channel_count; i++)
    {
        fill_gamma_ramp(shm_data + gamma_size * i, components[i], gamma);
    }
}

static void display_help()
{
    fprintf(stdout, "wlgamma [options]\n");
    fprintf(stdout, "\t-h\t\tDisplay this help information.\n");
    fprintf(stdout, "\t-v\t\tDisplay version information.\n\n");
    fprintf(stdout, "\t-l\t\tList all Wayland outputs and exit.\n");
    fprintf(stdout, "\t-o index\tTarget a particular Wayland output.\n\n");
    fprintf(stdout, "\t-r value\tSet the red channel multiplier (default: 1.0).\n");
    fprintf(stdout, "\t-g value\tSet the green channel multiplier (default: 1.0).\n");
    fprintf(stdout, "\t-b value\tSet the blue channel multiplier (default: 1.0).\n");
    fprintf(stdout, "\t-G value\tSet the gamma (default: 1.0).\n");
}

static void display_version()
{
    fprintf(stdout, "%s %i.%i.%i\n", program_name, major_version, minor_version, patch_version);
    fprintf(stdout, "Copyright 2025 Amini Allight\n\n");
    fprintf(stdout, "This program comes with ABSOLUTELY NO WARRANTY; This is free software, and you are welcome to redistribute it under certain conditions. See the included license for further details.\n");
}

static int display_outputs()
{
    fprintf(stdout, "Outputs:\n");

    display = wl_display_connect(NULL);

    if (!display)
    {
        fprintf(stderr, "Failed to connect to Wayland server.\n");
        return 1;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);
    wl_display_roundtrip(display);

    if (output_count == 0)
    {
        fprintf(stdout, "\tNo outputs available.");
    }

    for (int i = 0; i < output_count; i++)
    {
        fprintf(stdout, "\t%i: %s\n", i, output_names[i] ? output_names[i] : "No output name provided.");
    }

    return 0;
}

int main(int argc, char** argv)
{
    int result;
    int retcode = 0;
    int output_index = 0;
    float components[channel_count];
    float gamma = 1;
    char* end_pointer;

    signal(SIGINT, on_interrupt);

    for (int i = 0; i < channel_count; i++)
    {
        components[i] = 1;
    }

    int opt;
    while ((opt = getopt(argc, argv, "hvlo:r:g:b:G:")) != -1)
    {
        switch (opt)
        {
        case 'h' :
            display_help();
            goto end;
        case 'v' :
            display_version();
            goto end;
        case 'l' :
            retcode = display_outputs();
            goto end;
        case 'o' :
            output_index = strtoul(optarg, &end_pointer, 10);

            if (end_pointer == optarg)
            {
                fprintf(stderr, "Invalid output index '%s' supplied.\n", optarg);
                retcode = 1;
                goto end;
            }
            break;
        case 'r' :
            components[0] = strtof(optarg, &end_pointer);

            if (end_pointer == optarg)
            {
                fprintf(stderr, "Invalid red component '%s' supplied.\n", optarg);
                retcode = 1;
                goto end;
            }
            break;
        case 'g' :
            components[1] = strtof(optarg, &end_pointer);

            if (end_pointer == optarg)
            {
                fprintf(stderr, "Invalid green component '%s' supplied.\n", optarg);
                retcode = 1;
                goto end;
            }
            break;
        case 'b' :
            components[2] = strtof(optarg, &end_pointer);

            if (end_pointer == optarg)
            {
                fprintf(stderr, "Invalid blue component '%s' supplied.\n", optarg);
                retcode = 1;
                goto end;
            }
            break;
        case 'G' :
            gamma = strtof(optarg, &end_pointer);

            if (end_pointer == optarg)
            {
                fprintf(stderr, "Invalid gamma value '%s' supplied.\n", optarg);
                retcode = 1;
                goto end;
            }
            break;
        }
    }

    int no_effect = gamma == 1;

    for (int i = 0; i < channel_count; i++)
    {
        no_effect &= components[i] == 1;
    }

    if (no_effect)
    {
        display_help();
        goto end;
    }

    display = wl_display_connect(NULL);

    if (!display)
    {
        fprintf(stderr, "Failed to connect to Wayland server.\n");
        retcode = 1;
        goto end;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!gamma_control_manager)
    {
        fprintf(stderr, "The Wayland server does not provide zwlr_gamma_control_manager_v1.\n");
        retcode = 1;
        goto end;
    }

    if (output_count == 0)
    {
        fprintf(stderr, "The Wayland server did not provide any outputs.\n");
        retcode = 1;
        goto end;
    }

    if (output_count <= output_index)
    {
        fprintf(stderr, "The Wayland server did not provide output with index '%i'.\n", output_index);
        retcode = 1;
        goto end;
    }

    wl_display_roundtrip(display);

    gamma_control = zwlr_gamma_control_manager_v1_get_gamma_control(gamma_control_manager, outputs[output_index]);

    if (!gamma_control)
    {
        fprintf(stderr, "Failed to create a zwlr_gamma_control_v1.\n");
        retcode = 1;
        goto end;
    }

    zwlr_gamma_control_v1_add_listener(gamma_control, &gamma_control_listener, NULL);
    wl_display_roundtrip(display);

    if (gamma_size == 0)
    {
        fprintf(stderr, "Failed to get gamma table size.\n");
        retcode = 1;
        goto end;
    }

    shm_fd = shm_open(shm_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

    if (shm_fd < 0)
    {
        fprintf(stderr, "Failed to create shared memory: %i.\n", errno);
        retcode = 1;
        goto end;
    }

    result = shm_unlink(shm_path);

    if (result < 0)
    {
        fprintf(stderr, "Failed to unlink shared memory: %i.\n", errno);
        retcode = 1;
        goto end;
    }

    result = ftruncate(shm_fd, gamma_table_size());

    if (result < 0)
    {
        fprintf(stderr, "Failed to resize shared memory: %i.\n", errno);
        retcode = 1;
        goto end;
    }

    shm_data = mmap(NULL, gamma_table_size(), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (shm_data < 0)
    {
        fprintf(stderr, "Failed to map shared memory: %i.\n", errno);
        retcode = 1;
        goto end;
    }

    fill_gamma_table(components, gamma);

    zwlr_gamma_control_v1_set_gamma(gamma_control, shm_fd);

    while (!quit)
    {
        quit |= wl_display_roundtrip(display) == -1;
    }

end:
    if (shm_data > 0)
    {
        munmap(shm_data, gamma_table_size());
    }

    if (shm_fd >= 0)
    {
        close(shm_fd);
    }

    for (int i = 0; i < output_count; i++)
    {
        free(output_names[i]);
        wl_output_destroy(outputs[i]);
    }

    free(output_names);
    free(outputs);

    if (gamma_control)
    {
        zwlr_gamma_control_v1_destroy(gamma_control);
    }

    if (gamma_control_manager)
    {
        zwlr_gamma_control_manager_v1_destroy(gamma_control_manager);
    }

    if (registry)
    {
        wl_registry_destroy(registry);
    }

    if (display)
    {
        wl_display_disconnect(display);
    }

    return retcode;
}
