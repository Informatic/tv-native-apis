#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <jpeglib.h>

#include <vtcapture/vtCaptureApi_c.h>


const VT_CALLER_T caller[24] = "com.webos.tbtest.cap";

VT_DRIVER *driver;
VT_CLIENTID_T client[128] = "00";

_LibVtCaptureProperties props;

_LibVtCapturePlaneInfo plane;
int stride, x, y, w, h, xa, ya, wa, ha;
VT_REGION_T region;
VT_REGION_T activeregion;

_LibVtCaptureBufferInfo buff;
char *addr0, *addr1;
int size0, size1;
char *rgbout;

int isrunning = 0;

void NV21_TO_RGB24(unsigned char *yuyv, unsigned char *rgb, int width, int height);
int stop();
int finalize();
void write_JPEG_stdout(int quality);

#define CRLF() fwrite("\r\n", 1, 2, stdout)

int main(int argc, char *argv[])
{
    int done;
    int ex;
    //Capture properties for vtCapture_preprocess - _LibVtCaptureProperties
    int dumping = 2;
    int capturex = 0;
    int capturey = 0;
    int captureWidth = 1280;
    int captureHeight = 720;
    int framerate = 30;
    int buffer_count = 1;

    VT_DUMP_T dump = dumping;
    VT_LOC_T loc = {capturex, capturey};
    VT_RESOLUTION_T reg = {captureWidth, captureHeight};
    VT_FRAMERATE_T frm = framerate;
    VT_BUF_T buf_cnt = buffer_count;

    props.dump = dump;
    props.loc = loc;
    props.reg = reg;
    props.buf_cnt = buf_cnt;
    props.frm = framerate;
    //End of capture properties for vtCapture_preprocess - _LibVtCaptureProperties

    printf("HTTP/1.1 200 OK");
    CRLF();
    printf("Content-Type: multipart/x-mixed-replace; boundary=myboundary");
    CRLF();
    printf("Cache-Control: no-cache");
    CRLF();
    printf("Connection: close");
    CRLF();
    printf("Pragma: no-cache");
    CRLF();
    CRLF();

    fprintf(stderr, "Starting vtCapture using API: %dx%d @%dFPS\n", captureWidth, captureHeight,framerate);
    fprintf(stderr, "Used settings: dump location: %d, x: %d, y: %d, w: %d, h: %d, buf_cnt: %d, framerate: %d\n", dumping, capturex, capturey, captureWidth, captureHeight, framerate, buffer_count);

    driver = vtCapture_create();
    fprintf(stderr, "Driver created!\n");

    done = vtCapture_init(driver, caller, client);
    if (done != 0) {
        fprintf(stderr, "vtCapture_init failed: %x\nQuitting...\n", done);
        ex = finalize();
        return ex;
    }
    fprintf(stderr, "vtCapture_init done!\nCaller_ID: %s Client ID: %s \n", caller, client);

    done = vtCapture_preprocess(driver, client, &props);
    if (done != 0) {
        fprintf(stderr, "vtCapture_preprocess failed: %x\nQuitting...\n", done);
        ex = finalize();
        return ex;
    }
    fprintf(stderr, "vtCapture_preprocess done!\n");

    //_LibVtCaptureCapabilityInfo *test;
    //done = vtCapture_capabilityInfo(driver, client, test);

    done = vtCapture_planeInfo(driver, client, &plane);
    if (done == 0 ) {
        stride = plane.stride;

        region = plane.planeregion;
        x = region.a, y = region.b, w = region.c, h = region.d;

        activeregion = plane.activeregion;
        xa = activeregion.a, ya = activeregion.b, wa = activeregion.c, ha = activeregion.d;
    }else{
        fprintf(stderr, "vtCapture_planeInfo failed: %x\nQuitting...\n", done);
        ex = finalize();
        return ex;
    }
    fprintf(stderr, "vtCapture_planeInfo done!\nstride: %d\nRegion: x: %d, y: %d, w: %d, h: %d\nActive Region: x: %d, y: %d w: %d h: %d\n", stride, x, y, w, h, xa, ya, wa, ha);

    done = vtCapture_process(driver, client);
    if (done == 0){
        isrunning = 1;
    }else{
        isrunning = 0;
        fprintf(stderr, "vtCapture_process failed: %x\nQuitting...\n", done);
        ex = finalize();
        return ex;
    }
    fprintf(stderr, "vtCapture_process done!\n");

    sleep(2);


        done = vtCapture_currentCaptureBuffInfo(driver, &buff);
        if (done == 0 ) {
            addr0 = buff.start_addr0;
            addr1 = buff.start_addr1;
            size0 = buff.size0;
            size1 = buff.size1;
        }else{
            fprintf(stderr, "vtCapture_currentCaptureBuffInfo failed: %x\nQuitting...\n", done);
            ex = finalize();
            return ex;
        }
        fprintf(stderr, "vtCapture_currentCaptureBuffInfo done!\naddr0: %p addr1: %p size0: %d size1: %d\n", addr0, addr1, size0, size1);

        
    do{
        //Combine two Image Buffers to one and convert to RGB24
        char *first, *secound;
        first = (char *) malloc(size0*sizeof(char));
        memcpy(first, addr0, size0);
        secound = (char *) malloc(size1*sizeof(char));
        memcpy(secound, addr1, size1);

        int comsize;
        comsize = size0+size1;
        char *combined;
        combined = (char *) malloc((comsize)*sizeof(char));

        memcpy(combined, first, size0);
        memcpy(combined+size0, secound, size1);

        free(first);
        free(secound);

        int rgbsize = sizeof(combined)*w*h*3;
        rgbout = (char *) malloc(rgbsize);
        NV21_TO_RGB24(combined, rgbout, w, h);

        write_JPEG_stdout(85);


    /*
        FILE * pFile;
        pFile = fopen ("/tmp/myfile.bin","wb");
        if (pFile!=NULL)
        {
            fwrite(rgbout, rgbsize, sizeof(rgbout), pFile);
            fclose (pFile);
        }
    */
        free(rgbout);
        free(combined);
    }while(1==1);

    done = stop();
    return done;
}

//Credits: https://www.programmersought.com/article/18954751423/
void NV21_TO_RGB24(unsigned char *yuyv, unsigned char *rgb, int width, int height)
{
        const int nv_start = width * height ;
        int  index = 0, rgb_index = 0;
        uint8_t y, u, v;
        int r, g, b, nv_index = 0,i, j;
 
        for(i = 0; i < height; i++){
            for(j = 0; j < width; j ++){
                nv_index = i / 2  * width + j - j % 2;
 
                y = yuyv[rgb_index];
                u = yuyv[nv_start + nv_index ];
                v = yuyv[nv_start + nv_index + 1];
 
                r = y + (140 * (v-128))/100;
                g = y - (34 * (u-128))/100 - (71 * (v-128))/100;
                b = y + (177 * (u-128))/100;
 
                if(r > 255)   r = 255;
                if(g > 255)   g = 255;
                if(b > 255)   b = 255;
                if(r < 0)     r = 0;
                if(g < 0)     g = 0;
                if(b < 0)     b = 0;
 
                index = rgb_index % width + (height - i - 1) * width;
 
                //Front image
                rgb[i * width * 3 + 3 * j + 0] = r;
                rgb[i * width * 3 + 3 * j + 1] = g;
                rgb[i * width * 3 + 3 * j + 2] = b;
                
 
                rgb_index++;
            }
        }
}


int stop()
{
    int done;

    isrunning = 0;
    done = vtCapture_stop(driver, client);
    if (done != 0)
    {
        fprintf(stderr, "vtCapture_stop failed: %x\nQuitting...\n", done);
        return done;
    }
    fprintf(stderr, "vtCapture_stop done!\n");
    done = finalize();
    return done;
}

int finalize()
{
    fprintf(stderr, "-- Quit called! --\n");
    int done;
    done = vtCapture_postprocess(driver, client);
        if (done == 0){
            fprintf(stderr, "Quitting: vtCapture_postprocess done!\n");
            done = vtCapture_finalize(driver, client);
            if (done == 0) {
                fprintf(stderr, "Quitting: vtCapture_finalize done!\n");
                vtCapture_release(driver);
                fprintf(stderr, "Quitting: Driver released!\n");
                memset(&client,0,127);
                fprintf(stderr, "Quitting!\n");
                return -1;
            }
            fprintf(stderr, "Quitting: vtCapture_finalize failed: %x\n", done);
        }
    vtCapture_finalize(driver, client);
    vtCapture_release(driver);
    fprintf(stderr, "Quitting with errors: %x!\n", done);
    return 0;
}

//Credits: https://github.com/webosbrew/tv-native-apis/blob/main/samples/vt/capture/capture.c
void write_JPEG_stdout(int quality)
{
    /* This struct contains the JPEG compression parameters and pointers to
   * working space (which is allocated as needed by the JPEG library).
   * It is possible to have several such structures, representing multiple
   * compression/decompression processes, in existence at once.  We refer
   * to any one struct (and its associated working data) as a "JPEG object".
   */
    struct jpeg_compress_struct cinfo;
    /* This struct represents a JPEG error handler.  It is declared separately
   * because applications often want to supply a specialized error handler
   * (see the second half of this file for an example).  But here we just
   * take the easy way out and use the standard error handler, which will
   * print a message on stderr and call exit() if compression fails.
   * Note that this struct must live as long as the main JPEG parameter
   * struct, to avoid dangling-pointer problems.
   */
    struct jpeg_error_mgr jerr;
    /* More stuff */

    JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
    int row_stride;          /* physical row width in image buffer */

    /* Step 1: allocate and initialize JPEG compression object */

    /* We have to set up the error handler first, in case the initialization
   * step fails.  (Unlikely, but it could happen if you are out of memory.)
   * This routine fills in the contents of struct jerr, and returns jerr's
   * address which we place into the link field in cinfo.
   */
    cinfo.err = jpeg_std_error(&jerr);
    /* Now we can initialize the JPEG compression object. */
    jpeg_create_compress(&cinfo);

    unsigned long jpegSize = 0;
    unsigned char *jpegBuf = NULL;

    /* Step 2: specify data destination (eg, a file) */
    /* Note: steps 2 and 3 can be done in either order. */

    /* Here we use the library-supplied code to send compressed data to a
   * stdio stream.  You can also write your own code to do something else.
   * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
   * requires it in order to write binary files.
   */
    jpeg_mem_dest(&cinfo, &jpegBuf, &jpegSize);

    /* Step 3: set parameters for compression */

    /* First we supply a description of the input image.
   * Four fields of the cinfo struct must be filled in:
   */
    cinfo.image_width = w; /* image width and height, in pixels */
    cinfo.image_height = h;
    cinfo.input_components = 3;          /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB; /* colorspace of input image */
    /* Now use the library's routine to set default compression parameters.
   * (You must set at least cinfo.in_color_space before calling this,
   * since the defaults depend on the source color space.)
   */
    jpeg_set_defaults(&cinfo);
    /* Now you can set any non-default parameters you wish to.
   * Here we just illustrate the use of quality (quantization table) scaling:
   */
    jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

    /* Step 4: Start compressor */

    /* TRUE ensures that we will write a complete interchange-JPEG file.
   * Pass TRUE unless you are very sure of what you're doing.
   */
    jpeg_start_compress(&cinfo, TRUE);

    /* Step 5: while (scan lines remain to be written) */
    /*           jpeg_write_scanlines(...); */

    /* Here we use the library's state variable cinfo.next_scanline as the
   * loop counter, so that we don't have to keep track ourselves.
   * To keep things simple, we pass one scanline per call; you can pass
   * more if you wish, though.
   */
    row_stride = w * 3; /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height)
    {
        /* jpeg_write_scanlines expects an array of pointers to scanlines.
     * Here the array is only one element long, but you could pass
     * more than one scanline at a time if that's more convenient.
     */
        row_pointer[0] = &rgbout[(h - cinfo.next_scanline - 1) * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    /* Step 6: Finish compression */

    jpeg_finish_compress(&cinfo);
    /* After finish_compress, we can close the output file. */

    printf("--myboundary");
    CRLF();
    printf("Content-Type: image/jpeg");
    CRLF();
    printf("Content-Length: %d", jpegSize);
    CRLF();
    CRLF();
    fwrite(jpegBuf, 1, jpegSize, stdout);
//    FILE * pFile;
//    pFile = fopen ("/tmp/myfile.jpg","wb");
//    if (pFile!=NULL)
//    {
//        fwrite(jpegBuf, 1, jpegSize, pFile);
//        fclose (pFile);
//    }

    CRLF();

    /* Step 7: release JPEG compression object */

    /* This is an important step since it will release a good deal of memory. */
    jpeg_destroy_compress(&cinfo);

    /* And we're done! */
    // Free jpeg memory
    free(jpegBuf);
}