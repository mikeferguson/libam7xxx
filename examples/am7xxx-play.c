/*
 * am7xxx-play - play stuff on an am7xxx device (e.g. Acer C110, PicoPix 1020)
 *
 * Copyright (C) 2012  Antonio Ospite <ospite@studenti.unina.it>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @example examples/am7xxx-play.c
 * am7xxx-play uses libavdevice, libavformat, libavcodec and libswscale to
 * decode the input, encode it to jpeg and display it with libam7xxx.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>

#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <am7xxx.h>

/* On some systems ENOTSUP is not defined, fallback to its value on
 * linux which is equal to EOPNOTSUPP which is 95
 */
#ifndef ENOTSUP
#define ENOTSUP 95
#endif

static unsigned int run = 1;

struct video_input_ctx {
	AVFormatContext *format_ctx;
	AVCodecContext  *codec_ctx;
	int video_stream_index;
};

static int video_input_init(struct video_input_ctx *input_ctx,
			    const char *input_format_string,
			    const char *input_path,
			    AVDictionary **input_options)
{
	AVInputFormat *input_format = NULL;
	AVFormatContext *input_format_ctx;
	AVCodecContext *input_codec_ctx;
	AVCodec *input_codec;
	int video_index;
	unsigned int i;
	int ret;

	avdevice_register_all();
	avcodec_register_all();
	av_register_all();

	if (input_format_string) {
		/* find the desired input format */
		input_format = av_find_input_format(input_format_string);
		if (input_format == NULL) {
			fprintf(stderr, "cannot find input format\n");
			ret = -ENODEV;
			goto out;
		}
	}

	if (input_path == NULL) {
		fprintf(stderr, "input_path must not be NULL!\n");
		ret = -EINVAL;
		goto out;
	}

	/* open the input format/device */
	input_format_ctx = NULL;
	ret = avformat_open_input(&input_format_ctx,
				  input_path,
				  input_format,
				  input_options);
	if (ret < 0) {
		fprintf(stderr, "cannot open input format/device\n");
		goto out;
	}

	/* get information on the input stream (e.g. format, bitrate, framerate) */
	ret = avformat_find_stream_info(input_format_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "cannot get information on the stream\n");
		goto cleanup;
	}

	/* dump what was found */
	av_dump_format(input_format_ctx, 0, input_path, 0);

	/* look for the first video_stream */
	video_index = -1;
	for (i = 0; i < input_format_ctx->nb_streams; i++)
		if (input_format_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_index = i;
			break;
		}
	if (video_index == -1) {
		fprintf(stderr, "cannot find any video streams\n");
		ret = -ENOTSUP;
		goto cleanup;
	}

	/* get a pointer to the codec context for the video stream */
	input_codec_ctx = input_format_ctx->streams[video_index]->codec;
	if (input_codec_ctx == NULL) {
		fprintf(stderr, "input codec context is not valid\n");
		ret = -ENOTSUP;
		goto cleanup;
	}

	/* find the decoder for the video stream */
	input_codec = avcodec_find_decoder(input_codec_ctx->codec_id);
	if (input_codec == NULL) {
		fprintf(stderr, "input_codec is NULL!\n");
		ret = -ENOTSUP;
		goto cleanup;
	}

	/* open the decoder */
	ret = avcodec_open2(input_codec_ctx, input_codec, NULL);
	if (ret < 0) {
		fprintf(stderr, "cannot open input codec\n");
		ret = -ENOTSUP;
		goto cleanup;
	}

	input_ctx->format_ctx = input_format_ctx;
	input_ctx->codec_ctx = input_codec_ctx;
	input_ctx->video_stream_index = video_index;

	ret = 0;
	goto out;

cleanup:
	avformat_close_input(&input_format_ctx);
out:
	av_dict_free(input_options);
	*input_options = NULL;
	return ret;
}


struct video_output_ctx {
	AVCodecContext  *codec_ctx;
	int raw_output;
};

static int video_output_init(struct video_output_ctx *output_ctx,
			     struct video_input_ctx *input_ctx,
			     unsigned int upscale,
			     unsigned int quality,
			     am7xxx_image_format image_format,
			     am7xxx_device *dev)
{
	AVCodecContext *output_codec_ctx;
	AVCodec *output_codec;
	unsigned int new_output_width;
	unsigned int new_output_height;
	int ret;

	if (input_ctx == NULL) {
		fprintf(stderr, "input_ctx must not be NULL!\n");
		ret = -EINVAL;
		goto out;
	}

	/* create the encoder context */
	output_codec_ctx = avcodec_alloc_context3(NULL);
	if (output_codec_ctx == NULL) {
		fprintf(stderr, "cannot allocate output codec context!\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Calculate the new output dimension so the original picture is shown
	 * in its entirety */
	ret = am7xxx_calc_scaled_image_dimensions(dev,
						  upscale,
						  (input_ctx->codec_ctx)->width,
						  (input_ctx->codec_ctx)->height,
						  &new_output_width,
						  &new_output_height);
	if (ret < 0) {
		fprintf(stderr, "cannot calculate output dimension\n");
		goto cleanup;
	}

	/* put sample parameters */
	output_codec_ctx->bit_rate   = (input_ctx->codec_ctx)->bit_rate;
	output_codec_ctx->width      = new_output_width;
	output_codec_ctx->height     = new_output_height;
	output_codec_ctx->time_base.num  =
		(input_ctx->format_ctx)->streams[input_ctx->video_stream_index]->time_base.num;
	output_codec_ctx->time_base.den  =
		(input_ctx->format_ctx)->streams[input_ctx->video_stream_index]->time_base.den;

	/* When the raw format is requested we don't actually need to setup
	 * and open a decoder
	 */
	if (image_format == AM7XXX_IMAGE_FORMAT_NV12) {
		fprintf(stdout, "using raw output format\n");
		output_codec_ctx->pix_fmt    = PIX_FMT_NV12;
		output_ctx->codec_ctx = output_codec_ctx;
		output_ctx->raw_output = 1;
		ret = 0;
		goto out;
	}

	output_codec_ctx->pix_fmt    = PIX_FMT_YUVJ420P;
	output_codec_ctx->codec_id   = CODEC_ID_MJPEG;
	output_codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;

	/* Set quality and other VBR settings */

	/* @note: 'quality' is expected to be between 1 and 100, but a value
	 * between 0 to 99 has to be passed when calculating qmin and qmax.
	 * This way qmin and qmax will cover the range 1-FF_QUALITY_SCALE, and
	 * in particular they won't be 0, this is needed because they are used
	 * as divisor somewhere in the encoding process */
	output_codec_ctx->qmin       = output_codec_ctx->qmax = ((100 - (quality - 1)) * FF_QUALITY_SCALE) / 100;
	output_codec_ctx->mb_lmin    = output_codec_ctx->lmin = output_codec_ctx->qmin * FF_QP2LAMBDA;
	output_codec_ctx->mb_lmax    = output_codec_ctx->lmax = output_codec_ctx->qmax * FF_QP2LAMBDA;
	output_codec_ctx->flags      |= CODEC_FLAG_QSCALE;
	output_codec_ctx->global_quality = output_codec_ctx->qmin * FF_QP2LAMBDA;

	/* find the encoder */
	output_codec = avcodec_find_encoder(output_codec_ctx->codec_id);
	if (output_codec == NULL) {
		fprintf(stderr, "cannot find output codec!\n");
		ret = -ENOTSUP;
		goto cleanup;
	}

	/* open the codec */
	ret = avcodec_open2(output_codec_ctx, output_codec, NULL);
	if (ret < 0) {
		fprintf(stderr, "could not open output codec!\n");
		goto cleanup;
	}

	output_ctx->codec_ctx = output_codec_ctx;
	output_ctx->raw_output = 0;

	ret = 0;
	goto out;

cleanup:
	avcodec_close(output_codec_ctx);
	av_free(output_codec_ctx);
out:
	return ret;
}


static int am7xxx_play(const char *input_format_string,
		       AVDictionary **input_options,
		       const char *input_path,
		       unsigned int rescale_method,
		       unsigned int upscale,
		       unsigned int quality,
		       am7xxx_image_format image_format,
		       am7xxx_device *dev)
{
	struct video_input_ctx input_ctx;
	struct video_output_ctx output_ctx;
	AVFrame *picture_raw;
	AVFrame *picture_scaled;
	int out_buf_size;
	uint8_t *out_buf;
	int out_picture_size;
	struct SwsContext *sw_scale_ctx;
	AVPacket packet;
	int got_picture;
	int ret = 0;

	ret = video_input_init(&input_ctx, input_format_string, input_path, input_options);
	if (ret < 0) {
		fprintf(stderr, "cannot initialize input\n");
		goto out;
	}

	ret = video_output_init(&output_ctx, &input_ctx, upscale, quality, image_format, dev);
	if (ret < 0) {
		fprintf(stderr, "cannot initialize input\n");
		goto cleanup_input;
	}

	/* allocate an input frame */
	picture_raw = avcodec_alloc_frame();
	if (picture_raw == NULL) {
		fprintf(stderr, "cannot allocate the raw picture frame!\n");
		ret = -ENOMEM;
		goto cleanup_output;
	}

	/* allocate output frame */
	picture_scaled = avcodec_alloc_frame();
	if (picture_scaled == NULL) {
		fprintf(stderr, "cannot allocate the scaled picture!\n");
		ret = -ENOMEM;
		goto cleanup_picture_raw;
	}

	/* calculate the bytes needed for the output image and create buffer for the output image */
	out_buf_size = avpicture_get_size((output_ctx.codec_ctx)->pix_fmt,
					  (output_ctx.codec_ctx)->width,
					  (output_ctx.codec_ctx)->height);
	out_buf = av_malloc(out_buf_size * sizeof(uint8_t));
	if (out_buf == NULL) {
		fprintf(stderr, "cannot allocate output data buffer!\n");
		ret = -ENOMEM;
		goto cleanup_picture_scaled;
	}

	/* assign appropriate parts of buffer to image planes in picture_scaled */
	avpicture_fill((AVPicture *)picture_scaled,
		       out_buf,
		       (output_ctx.codec_ctx)->pix_fmt,
		       (output_ctx.codec_ctx)->width,
		       (output_ctx.codec_ctx)->height);

	sw_scale_ctx = sws_getCachedContext(NULL,
					    (input_ctx.codec_ctx)->width,
					    (input_ctx.codec_ctx)->height,
					    (input_ctx.codec_ctx)->pix_fmt,
					    (output_ctx.codec_ctx)->width,
					    (output_ctx.codec_ctx)->height,
					    (output_ctx.codec_ctx)->pix_fmt,
					    rescale_method,
					    NULL, NULL, NULL);
	if (sw_scale_ctx == NULL) {
		fprintf(stderr, "cannot set up the rescaling context!\n");
		ret = -EINVAL;
		goto cleanup_out_buf;
	}

	while (run) {
		/* read packet */
		ret = av_read_frame(input_ctx.format_ctx, &packet);
		if (ret < 0) {
			if (ret == (int)AVERROR_EOF || input_ctx.format_ctx->pb->eof_reached)
				ret = 0;
			else
				fprintf(stderr, "av_read_frame failed, EOF?\n");
			run = 0;
			goto end_while;
		}

		if (packet.stream_index != input_ctx.video_stream_index) {
			/* that is more or less a "continue", but there is
			 * still the packet to free */
			goto end_while;
		}

		/* decode */
		got_picture = 0;
		ret = avcodec_decode_video2(input_ctx.codec_ctx, picture_raw, &got_picture, &packet);
		if (ret < 0) {
			fprintf(stderr, "cannot decode video\n");
			run = 0;
			goto end_while;
		}

		/* if we get the complete frame */
		if (got_picture) {
			/* convert it to YUV */
			sws_scale(sw_scale_ctx,
				  (const uint8_t * const*)picture_raw->data,
				  picture_raw->linesize,
				  0,
				  (input_ctx.codec_ctx)->height,
				  picture_scaled->data,
				  picture_scaled->linesize);

			if (output_ctx.raw_output) {
				out_picture_size = out_buf_size;
			} else {
				picture_scaled->quality = (output_ctx.codec_ctx)->global_quality;
				/* TODO: switch to avcodec_encode_video2() eventually */
				out_picture_size = avcodec_encode_video(output_ctx.codec_ctx,
									out_buf,
									out_buf_size,
									picture_scaled);
				if (out_picture_size < 0) {
					fprintf(stderr, "cannot encode video\n");
					ret = out_picture_size;
					run = 0;
					goto end_while;
				}
			}

#ifdef DEBUG
			char filename[NAME_MAX];
			FILE *file;
			if (!output_ctx.raw_output)
				snprintf(filename, NAME_MAX, "out_q%03d.jpg", quality);
			else
				snprintf(filename, NAME_MAX, "out.raw");
			file = fopen(filename, "wb");
			fwrite(out_buf, 1, out_picture_size, file);
			fclose(file);
#endif

			ret = am7xxx_send_image(dev,
						image_format,
						(output_ctx.codec_ctx)->width,
						(output_ctx.codec_ctx)->height,
						out_buf,
						out_picture_size);
			if (ret < 0) {
				perror("am7xxx_send_image");
				run = 0;
				goto end_while;
			}
		}
end_while:
		av_free_packet(&packet);
	}

	sws_freeContext(sw_scale_ctx);
cleanup_out_buf:
	av_free(out_buf);
cleanup_picture_scaled:
	av_free(picture_scaled);
cleanup_picture_raw:
	av_free(picture_raw);

cleanup_output:
	/* av_free is needed as well,
	 * see http://libav.org/doxygen/master/avcodec_8h.html#a5d7440cd7ea195bd0b14f21a00ef36dd
	 */
	avcodec_close(output_ctx.codec_ctx);
	av_free(output_ctx.codec_ctx);

cleanup_input:
	avcodec_close(input_ctx.codec_ctx);
	avformat_close_input(&(input_ctx.format_ctx));

out:
	return ret;
}

#ifdef HAVE_XCB
#include <xcb/xcb.h>
static int x_get_screen_dimensions(const char *displayname, int *width, int *height)
{
	int i, screen_number;
	xcb_connection_t *connection;
	const xcb_setup_t *setup;
	xcb_screen_iterator_t iter;

	connection = xcb_connect(displayname, &screen_number);
	if (xcb_connection_has_error(connection)) {
		fprintf(stderr, "Cannot open a connection to %s\n", displayname);
		return -EINVAL;
	}

	setup = xcb_get_setup(connection);
	if (setup == NULL) {
		fprintf(stderr, "Cannot get setup for %s\n", displayname);
		xcb_disconnect(connection);
		return -EINVAL;
	}

	iter = xcb_setup_roots_iterator(setup);
	for (i = 0; i < screen_number; ++i) {
		xcb_screen_next(&iter);
	}

	xcb_screen_t *screen = iter.data;

	*width = screen->width_in_pixels;
	*height = screen->height_in_pixels;

	xcb_disconnect(connection);

	return 0;
}

static char *get_x_screen_size(const char *input_path)
{
	int len;
	int width;
	int height;
	char *screen_size;
	int ret;

	ret = x_get_screen_dimensions(input_path, &width, &height);
	if (ret < 0) {
		fprintf(stderr, "Cannot get screen dimensions for %s\n", input_path);
		return NULL;
	}

	len = snprintf(NULL, 0, "%dx%d", width, height);

	screen_size = malloc((len + 1) * sizeof(char));
	if (screen_size == NULL) {
		perror("malloc");
		return NULL;
	}

	len = snprintf(screen_size, len + 1, "%dx%d", width, height);
	if (len < 0) {
		free(screen_size);
		screen_size = NULL;
		return NULL;
	}
	return screen_size;
}
#else
static char *get_x_screen_size(const char *input_path)
{
	(void) input_path;
	fprintf(stderr, "%s: fallback implementation\n", __func__);
	return strdup("vga");
}
#endif

static void unset_run(int signo)
{
	(void) signo;
	run = 0;
}

#ifdef HAVE_SIGACTION
static int set_signal_handler(void (*signal_handler)(int))
{
	struct sigaction new_action;
	struct sigaction old_action;
	int ret = 0;

	new_action.sa_handler = signal_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags = 0;

	ret = sigaction(SIGINT, NULL, &old_action);
	if (ret < 0) {
		perror("sigaction on old_action");
		goto out;
	}

	if (old_action.sa_handler != SIG_IGN) {
		ret = sigaction(SIGINT, &new_action, NULL);
		if (ret < 0) {
			perror("sigaction on new_action");
			goto out;
		}
	}

out:
	return ret;
}
#else
static int set_signal_handler(void (*signal_handler)(int))
{
	(void)signal_handler;
	fprintf(stderr, "set_signal_handler() not implemented, sigaction not available\n");
	return 0;
}
#endif


static void usage(char *name)
{
	printf("usage: %s [OPTIONS]\n\n", name);
	printf("OPTIONS:\n");
	printf("\t-f <input format>\tthe input device format\n");
	printf("\t-i <input path>\t\tthe input path\n");
	printf("\t-o <options>\t\ta comma separated list of input format options\n");
	printf("\t\t\t\tEXAMPLE:\n");
	printf("\t\t\t\t\t-o draw_mouse=1,framerate=100,video_size=800x480\n");
	printf("\t-s <scaling method>\tthe rescaling method (see swscale.h)\n");
	printf("\t-u \t\t\tupscale the image if smaller than the display dimensions\n");
	printf("\t-F <format>\t\tthe image format to use (default is JPEG)\n");
	printf("\t\t\t\tSUPPORTED FORMATS:\n");
	printf("\t\t\t\t\t1 - JPEG\n");
	printf("\t\t\t\t\t2 - NV12\n");
	printf("\t-q <quality>\t\tquality of jpeg sent to the device, between 1 and 100\n");
	printf("\t-l <log level>\t\tthe verbosity level of libam7xxx output (0-5)\n");
	printf("\t-p <power mode>\t\tthe power mode of device, between %d (off) and %d (turbo)\n",
	       AM7XXX_POWER_OFF, AM7XXX_POWER_TURBO);
	printf("\t\t\t\tWARNING: Level 2 and greater require the master AND\n");
	printf("\t\t\t\t         the slave connector to be plugged in.\n");
	printf("\t-z <zoom mode>\t\tthe display zoom mode, between %d (original) and %d (test)\n",
	       AM7XXX_ZOOM_ORIGINAL, AM7XXX_ZOOM_TEST);
	printf("\t-h \t\t\tthis help message\n");
	printf("\n\nEXAMPLES OF USE:\n");
	printf("\t%s -f x11grab -i :0.0 -o video_size=800x480\n", name);
	printf("\t%s -f fbdev -i /dev/fb0\n", name);
	printf("\t%s -f video4linux2 -i /dev/video0 -o video_size=320x240,frame_rate=100 -u -q 90\n", name);
	printf("\t%s -i http://download.blender.org/peach/bigbuckbunny_movies/BigBuckBunny_640x360.m4v\n", name);
}

int main(int argc, char *argv[])
{
	int ret;
	int opt;
	char *subopts;
	char *subopts_saved;
	char *subopt;
	char *input_format_string = NULL;
	AVDictionary *options = NULL;
	char *input_path = NULL;
	unsigned int rescale_method = SWS_BICUBIC;
	unsigned int upscale = 0;
	unsigned int quality = 95;
	int log_level = AM7XXX_LOG_INFO;
	am7xxx_power_mode power_mode = AM7XXX_POWER_LOW;
	am7xxx_zoom_mode zoom = AM7XXX_ZOOM_ORIGINAL;
	int format = AM7XXX_IMAGE_FORMAT_JPEG;
	am7xxx_context *ctx;
	am7xxx_device *dev;

	while ((opt = getopt(argc, argv, "f:i:o:s:uF:q:l:p:z:h")) != -1) {
		switch (opt) {
		case 'f':
			input_format_string = strdup(optarg);
			break;
		case 'i':
			input_path = strdup(optarg);
			break;
		case 'o':
#ifdef HAVE_STRTOK_R
			/*
			 * parse suboptions, the expected format is something
			 * like:
			 *   draw_mouse=1,framerate=100,video_size=800x480
			 */
			subopts = subopts_saved = strdup(optarg);
			while((subopt = strtok_r(subopts, ",", &subopts))) {
				char *subopt_name = strtok_r(subopt, "=", &subopt);
				char *subopt_value = strtok_r(NULL, "", &subopt);
				if (subopt_value == NULL) {
					fprintf(stderr, "invalid suboption: %s\n", subopt_name);
					continue;
				}
				av_dict_set(&options, subopt_name, subopt_value, 0);
			}
			free(subopts_saved);
#else
			fprintf(stderr, "Option '-o' not implemented\n");
#endif
			break;
		case 's':
			rescale_method = atoi(optarg);
			switch(rescale_method) {
			case SWS_FAST_BILINEAR:
			case SWS_BILINEAR:
			case SWS_BICUBIC:
			case SWS_X:
			case SWS_POINT:
			case SWS_AREA:
			case SWS_BICUBLIN:
			case SWS_GAUSS:
			case SWS_SINC:
			case SWS_LANCZOS:
			case SWS_SPLINE:
				break;
			default:
				fprintf(stderr, "Unsupported rescale method\n");
				ret = -EINVAL;
				goto out;
			}
			break;
		case 'u':
			upscale = 1;
			break;
		case 'F':
			format = atoi(optarg);
			switch(format) {
			case AM7XXX_IMAGE_FORMAT_JPEG:
				fprintf(stdout, "JPEG format\n");
				break;
			case AM7XXX_IMAGE_FORMAT_NV12:
				fprintf(stdout, "NV12 format\n");
				break;
			default:
				fprintf(stderr, "Unsupported format\n");
				ret = -EINVAL;
				goto out;
			}
			break;
		case 'q':
			quality = atoi(optarg);
			if (quality < 1 || quality > 100) {
				fprintf(stderr, "Invalid quality value, must be between 1 and 100\n");
				ret = -EINVAL;
				goto out;
			}
			break;
		case 'l':
			log_level = atoi(optarg);
			if (log_level < AM7XXX_LOG_FATAL || log_level > AM7XXX_LOG_TRACE) {
				fprintf(stderr, "Unsupported log level, falling back to AM7XXX_LOG_ERROR\n");
				log_level = AM7XXX_LOG_ERROR;
			}
			break;
		case 'p':
			power_mode = atoi(optarg);
			switch(power_mode) {
			case AM7XXX_POWER_OFF:
			case AM7XXX_POWER_LOW:
			case AM7XXX_POWER_MIDDLE:
			case AM7XXX_POWER_HIGH:
			case AM7XXX_POWER_TURBO:
				fprintf(stdout, "Power mode: %d\n", power_mode);
				break;
			default:
				fprintf(stderr, "Invalid power mode value, must be between %d and %d\n",
					AM7XXX_POWER_OFF, AM7XXX_POWER_TURBO);
				ret = -EINVAL;
				goto out;
			}
			break;
		case 'z':
			zoom = atoi(optarg);
			switch(zoom) {
			case AM7XXX_ZOOM_ORIGINAL:
			case AM7XXX_ZOOM_H:
			case AM7XXX_ZOOM_H_V:
			case AM7XXX_ZOOM_TEST:
				fprintf(stdout, "Zoom: %d\n", zoom);
				break;
			default:
				fprintf(stderr, "Invalid zoom mode value, must be between %d and %d\n",
					AM7XXX_ZOOM_ORIGINAL, AM7XXX_ZOOM_TEST);
				exit(EXIT_FAILURE);
			}
			break;
		case 'h':
			usage(argv[0]);
			ret = 0;
			goto out;
			break;
		default: /* '?' */
			usage(argv[0]);
			ret = -EINVAL;
			goto out;
		}
	}

	if (input_path == NULL) {
		fprintf(stderr, "The -i option must always be passed\n");
		ret = -EINVAL;
		goto out;
	}

	/*
	 * When the input format is 'x11grab' set some useful fallback options
	 * if not supplied by the user, in particular grab full screen
	 */
	if (input_format_string && strcmp(input_format_string, "x11grab") == 0) {
		char *video_size;

		video_size = get_x_screen_size(input_path);

		if (!av_dict_get(options, "video_size", NULL, 0))
			av_dict_set(&options, "video_size", video_size, 0);

		if (!av_dict_get(options, "framerate", NULL, 0))
			av_dict_set(&options, "framerate", "60", 0);

		if (!av_dict_get(options, "draw_mouse", NULL, 0))
			av_dict_set(&options, "draw_mouse",  "1", 0);

		free(video_size);
	}

	ret = set_signal_handler(unset_run);
	if (ret < 0) {
		perror("sigaction");
		goto out;
	}

	ret = am7xxx_init(&ctx);
	if (ret < 0) {
		perror("am7xxx_init");
		goto out;
	}

	am7xxx_set_log_level(ctx, log_level);

	ret = am7xxx_open_device(ctx, &dev, 0);
	if (ret < 0) {
		perror("am7xxx_open_device");
		goto cleanup;
	}

	ret = am7xxx_set_zoom_mode(dev, zoom);
	if (ret < 0) {
		perror("am7xxx_set_zoom_mode");
		goto cleanup;
	}

	ret = am7xxx_set_power_mode(dev, power_mode);
	if (ret < 0) {
		perror("am7xxx_set_power_mode");
		goto cleanup;
	}

	/* When setting AM7XXX_ZOOM_TEST don't display the actual image */
	if (zoom == AM7XXX_ZOOM_TEST)
		goto cleanup;

	ret = am7xxx_play(input_format_string,
			  &options,
			  input_path,
			  rescale_method,
			  upscale,
			  quality,
			  format,
			  dev);
	if (ret < 0) {
		fprintf(stderr, "am7xxx_play failed\n");
		goto cleanup;
	}

cleanup:
	am7xxx_shutdown(ctx);
out:
	av_dict_free(&options);
	free(input_path);
	free(input_format_string);
	return ret;
}
