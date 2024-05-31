#define _DARWIN_BETTER_REALPATH 1
#include <signal.h>
#include <Drp/term_util.h>
#include <Drp/argument_parsing.h>
#include <Drp/file_util.h>
#include <Drp/long_string.h>
#include <Drp/get_input.h>
#include <Drp/parse_numbers.h>
#include <Drp/base64.h>
#include <time.h>
#define STBI_NEON 1
#define STB_IMAGE_IMPLEMENTATION 1
#include <Drp/stb/stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION 1
#include <Drp/stb/stb_image_resize.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Drp/stb/stb_image_write.h>

static
StringView imgpaths[1024*8];
static
StringView realpaths[1024*8];

static
int npaths = 0;

static
int current = 0;
static int width = 0, height = 0;

static double scale=0;
static _Bool auto_height = 0, auto_width = 0, auto_scale = 0;
static _Bool need_rescale = 1;


static
void
sighandler(int sig){
    if(sig == SIGWINCH)
        return need_rescale = 1, (void)0;
}

static
void rescale(void){
    TermSize sz = get_terminal_size();
    if(auto_width || auto_scale) width = sz.xpix;
    if(auto_height || auto_scale) height = sz.ypix*(sz.rows-2)/sz.rows;
    need_rescale = 0;
}
static
void
restore_buff(void){
    printf("\033[?1049l");
    fflush(stdout);
}
FILE* flog = NULL;
#define LOG(mess, ...) do { \
    if(!flog) \
        flog = fopen("debug.txt", "w"); \
    fprintf(flog, "%d: " mess "\n", __LINE__,##__VA_ARGS__); \
}while(0);

static void
write_func(void* ctx, void* d, int size){
    char* data = d;
    char b64buff[4096];
    _Bool first = 1;
    int m = 1;
    static int id = 13337;
    while(size > 0){
        int chunk = (sizeof b64buff)/4*3;
        if(chunk > size) {
            chunk = size;
            m = 0;
        }
        int b64_size = base64_encode_size(chunk);
        int ch = base64_encode(b64buff, sizeof b64buff, data, chunk);
        if(m == 0 && b64_size < sizeof b64buff){
            if((b64_size % 4) != 0) b64buff[b64_size++] = '=';
            if((b64_size % 4) != 0) b64buff[b64_size++] = '=';
            if((b64_size % 4) != 0) b64buff[b64_size++] = '=';
        }
        if(first){
            printf("\033[H\033[2J");
            printf("\033_Gf=100,a=t,i=%d,m=%d,q=1;",++id, m);
            first = 0;
        }
        else {
            printf("\033_Gm=%d;", m);
        }
        printf("%.*s\033\\", b64_size, b64buff);
        data += chunk;
        size -= chunk;
    }
    printf("\033_Ga=p,i=%d,q=1\033\\",id);
    printf("\033_Ga=d,d=i,i=%d,q=1\033\\",id-1);
    printf("\n\r");
    printf("\033\\\033[2K%d/%d\n", current+1, npaths);
    fflush(stdout);
}

int main(int argc, const char** argv){
    signal(SIGWINCH, sighandler);
    ArgToParse pos_args[] = {
        [0] = {
            .name = SV("imgs"),
            .help = "imgs to show",
            .dest = ARGDEST(imgpaths),
            .min_num = 1,
            .max_num = arrlen(imgpaths),
        },
    };
    ArgToParse kw_args[] = {
        {
            .name = SV("-w"),
            .altname1 = SV("--width"),
            .dest = ARGDEST(&width),
            .help = "Force image to this width.",
        },
        {
            .name = SV("-h"),
            .altname1 = SV("--height"),
            .dest = ARGDEST(&height),
            .help = "Force image to this height.",
        },
        {
            .name = SV("-s"),
            .altname1 = SV("--scale"),
            .dest = ARGDEST(&scale),
            .help = "Rescale image by this factor.",
        },
        {
            .name = SV("--start"),
            .dest = ARGDEST(&current),
            .help = "Show images starting from this number instead of the first one.",
        },
        {
            .name = SV("--auto-height"),
            .dest = ARGDEST(&auto_height),
            .help = "Rescale images to the height of the terminal.",
        },
        {
            .name = SV("--auto-width"),
            .dest = ARGDEST(&auto_width),
            .help = "Rescale images to the width of the terminal.",
        },
        {
            .name = SV("--auto"),
            .dest = ARGDEST(&auto_scale),
            .help = "Rescale images to the width or height of the terminal, whichever requires less scaling and will fit.",
        },
    };
    enum {HELP, HIDDEN_HELP, FISH};
    ArgToParse early_args[] = {
        [HELP] = {
            // .name = SV("-h"),
            .name = SV("--help"),
            .help = "Print this help and exit.",
        },
        [HIDDEN_HELP] = {
            .name = SV("-H"),
            .altname1 = SV("--hidden-help"),
            .help = "Print out help for the hidden arguments and exit.",
        },
        [FISH] = {
            .name = SV("--fish-completions"),
            .help = "Print out commands for fish shell completions.",
            .hidden = 1,
        },
    };
    Args args = {argc-1, argv+1};
    ArgParser parser = {
        .name = argv[0],
        .description = "img pager",
        .positional.args = pos_args,
        .positional.count = arrlen(pos_args),
        .keyword.args = kw_args,
        .keyword.count = arrlen(kw_args),
        .early_out.args = early_args,
        .early_out.count = arrlen(early_args),
    };
    TermSize sz = get_terminal_size();
    switch(check_for_early_out_args(&parser, &args)){
        case HELP:{
            int columns = sz.columns;
            if(columns > 80) columns = 80;
            print_argparse_help(&parser, columns);
            return 0;
        }
        case HIDDEN_HELP:{
            int columns = sz.columns;
            if(columns > 80) columns = 80;
            print_argparse_hidden_help(&parser, columns);
            return 0;
        }
        case FISH:{
            print_argparse_fish_completions(&parser);
            return 0;
        }
        default:
            break;
    }
    enum ArgParseError parse_err = parse_args(&parser, &args, ARGPARSE_FLAGS_NONE);
    if(parse_err){
        print_argparse_error(&parser, parse_err);
        return 1;
    }
    npaths = pos_args[0].num_parsed;
    if(!npaths) return 0;
    for(int i = 0; i < npaths; i++){
        const StringView* p = &imgpaths[i];
        StringView* rp = &realpaths[i];
        rp->text = realpath(p->text, NULL);
        rp->length = strlen(rp->text);
    }

    GetInputCtx input = {
        .prompt = SV(""),
    };
    char b64buff[4092];
    if(1){
        atexit(restore_buff);
        printf("\033[?1049h");
        fflush(stdout);
    }
    for(int i = 0; i < sz.rows; i++)
        puts("");
    rescale();

    goto show;
    for(;;){
        {
            fputs("\033[2K", stdout);
            fflush(stdout);
            int c;
            switch((c = gi_get_one_char())){
                case -1:
                    return 0;
                case '>':
                case '.':
                case '+':
                case 'n':
                case '\r':
                case ' ':
                    if(current < 0) current = 0;
                    if(current >= npaths) current = npaths-1;
                    current++;
                    goto show;
                case 'l':
                    printf("%.*s\n", (int)realpaths[current].length, realpaths[current].text);
                    continue;
                case '-':
                case '<':
                case ',':
                case 'p':
                    current--;
                    if(current < 0) current = 0;
                    if(current >= npaths) current = npaths-1;
                    goto show;
                case 'q':
                case 'x':
                case 4: // CTRL-D
                    return 0;
                case '0' ... '9':
                    input.buff[0] = (char)c;
                    break;
                default:
                    continue;
            }
            ssize_t len = gi_get_input2(&input, 1);
            fputs("\r\033[2K", stdout);
            fflush(stdout);
            if(len < 0) break;
            if(!len) continue;
            IntResult ir = parse_int(input.buff, len);
            if(ir.errored) continue;
            int pg = ir.result-1;
            if(pg < 0) pg = 0;
            if(pg >= npaths) pg = npaths-1;
            current = pg;
            goto show;
        }
        show:;
        if(current < 0) current = 0;
        if(current >= npaths) current = npaths-1;
        StringView path = realpaths[current];
        if(width || height || scale || auto_scale){
            if(need_rescale) rescale();
            int w = width, h = height;
            int x, y, n;
            uint8_t * data = NULL;
            uint8_t * data2 = NULL;
            uint8_t * data3 = NULL;
            data = stbi_load(path.text, &x, &y, &n, 0);
            if(!data) {
                printf("Failed to load %s\n", path.text);
                goto cleanup;
            }
            if(scale){
                w = (int)(scale*x);
                h = (int)(scale*y);
            }
            if(auto_scale){
                // This is wrong, as if the image is bigger than the screen it
                // picks the wrong one.
                double xratio = (double)width/(double)x;
                double yratio = (double)height/(double)y;
                if(xratio < yratio){
                    w = (int)(xratio * x);
                    h = (int)(xratio * y);
                }
                else {
                    w = (int)(yratio * x);
                    h = (int)(yratio * y);
                }
            }
            if(!w){
                double s = (double)h/(double)y;
                w = (int)(s*x);
            }
            if(!h){
                double s = (double)w/(double)x;
                h = (int)(s*y);
            }
            size_t data2_length = (size_t)w*(size_t)h*(size_t)n;
            data2 = malloc(data2_length);
            if(!data2) goto cleanup;
            int ok = stbir_resize_uint8(data, x, y, 0, data2, w, h, 0, n);
            if(!ok) {
                printf("Failed to resize %s\n", path.text);
                goto cleanup;
            }
            if(1){
                #if DO_TIMING
                    struct timespec t0, t1;
                    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
                #endif
                stbi_write_png_to_func(write_func, NULL, w, h, n, data2, 0);
                printf("%.*s\n", (int)imgpaths[current].length, imgpaths[current].text);
                #if DO_TIMING
                    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
                    printf("%.3fs\n", (double)t1.tv_sec+(double)t1.tv_nsec/1e9-(double)t0.tv_sec-(double)t0.tv_nsec/1e9);
                #endif
                goto cleanup;
            }

            #if DO_TIMING
                struct timespec t0, t1;
                clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
            #endif
            size_t b64_len = base64_encode_size(data2_length);
            data3 = malloc(b64_len);
            if(!data3) goto cleanup;
            size_t used = base64_encode((char*)data3, b64_len, data2, data2_length);
            if(!used) goto cleanup;
            uint8_t* to_write = data3;
            printf("\033[H\033[2J\033_Ga=d\033\\");
            printf("\033_Gf=%d,a=T,s=%d,v=%d,", n*8, w, h);
            enum {chunk_size=4096};
            // write first chunk differently
            {
                printf(used > chunk_size?"m=1;":"m=0;");
                size_t w_size = used >= chunk_size?chunk_size: used;
                size_t writ =  fwrite(to_write, w_size, 1, stdout);
                if(writ != 1){
                    // idk
                }
                printf("\033\\");
                to_write += w_size;
                used -= w_size;
            }
            while(used > chunk_size){
                printf("\033_Gm=1;");
                size_t writ =  fwrite(to_write, chunk_size, 1, stdout);
                if(writ != 1){
                    // idk
                }
                printf("\033\\");
                to_write += chunk_size;
                used -= chunk_size;
            }
            // last chunk
            if(used){
                printf("\033_Gm=0;");
                size_t writ =  fwrite(to_write, used, 1, stdout);
                if(writ != 1){
                    // idk
                }
                printf("\033\\");
                to_write += used;
                used -= used;
            }
            printf("\n\r");
            printf("\033\\\033[2K%d/%d\n", current+1, npaths);
            printf("%.*s\n", (int)imgpaths[current].length, imgpaths[current].text);
            fflush(stdout);
            #if DO_TIMING
                clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
                printf("%.3fs\n", (double)t1.tv_sec+(double)t1.tv_nsec/1e9-(double)t0.tv_sec-(double)t0.tv_nsec/1e9);
            #endif
            {
                cleanup:
                free(data);
                free(data2);
                free(data3);
            }
        }
        else {
            size_t used = base64_encode(b64buff, sizeof b64buff, path.text, path.length);
            if(used %4 != 0) b64buff[used++] = '=';
            if(used %4 != 0) b64buff[used++] = '=';
            if(used %4 != 0) b64buff[used++] = '=';
            printf("\033[H\033[2J\033_Ga=d\033\\");
            printf("\033_Ga=T,f=100,t=f,d=a,C=0;%.*s\033\\\n\r", (int)used, b64buff);
            printf("\033[2K%d/%d\n", current+1, npaths);
            printf("%.*s\n", (int)imgpaths[current].length, imgpaths[current].text);
            // printf("%.*s\n", (int)path.length, path.text);
            fflush(stdout);
        }
    }
}


#include <Drp/Allocators/allocator.c>
#include <Drp/get_input.c>
