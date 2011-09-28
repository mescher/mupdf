/*
 * pdfdraw -- command line tool for drawing pdf documents
 */

#include "fitz.h"
#include "mupdf.h"

#ifdef _MSC_VER
#include <winsock2.h>
#else
#include <sys/time.h>
#endif

char *output = NULL;
float resolution = 72;
float rotation = 0;

int showxml = 0;
int showtext = 0;
int showhtml = 0;
int showjson = 0;
int showtime = 0;
int showmd5 = 0;
int showpages = 0;
int savealpha = 0;
int uselist = 1;
int alphabits = 8;
float gamma_value = 1;
int invert = 0;

fz_colorspace *colorspace;
fz_glyph_cache *glyphcache;
char *filename;

struct {
	int count, total;
	int min, max;
	int minpage, maxpage;
} timing;

static void die(fz_error error)
{
	fz_catch(error, "aborting");
	exit(1);
}

static void usage(void)
{
	fprintf(stderr,
		"usage: generate [options] input.pdf [pages]\n"
		"\t-o -\toutput filename (%%d for page number)\n"
		// "\t\tsupported formats: pgm, ppm, pam, png, pbm\n"
		"\t-p -\tpassword\n"
		"\t-r -\tresolution in dpi (default: 72)\n"
		// "\t-A\tdisable accelerated functions\n"
		"\t-a\tsave alpha channel (only pam and png)\n"
		"\t-b -\tnumber of bits of antialiasing (0 to 8)\n"
		"\t-g\trender in grayscale\n"
		// "\t-m\tshow timing information\n"
		"\t-t\tshow text (-tt for xml -ttt for xml with merged chars)\n"
		"\t-h \tshow html\n"
		"\t-J \toutput in JSON format\n"
		// "\t-x\tshow display list\n"
		// "\t-d\tdisable use of display list\n"
		// "\t-5\tshow md5 checksums\n"
		"\t-n\t show the number of pages\n"
		"\t-R -\trotate clockwise by given number of degrees\n"
		"\t-G gamma\tgamma correct output\n"
		// "\t-I\tinvert output\n"
		"\tpages\tcomma separated list of ranges\n");
	exit(1);
}

static int gettime(void)
{
	static struct timeval first;
	static int once = 1;
	struct timeval now;
	if (once)
	{
		gettimeofday(&first, NULL);
		once = 0;
	}
	gettimeofday(&now, NULL);
	return (now.tv_sec - first.tv_sec) * 1000 + (now.tv_usec - first.tv_usec) / 1000;
}

static int isrange(char *s)
{
	while (*s)
	{
		if ((*s < '0' || *s > '9') && *s != '-' && *s != ',')
			return 0;
		s++;
	}
	return 1;
}

void drawpage(pdf_xref *xref, int pagenum)
{
	fz_error error;
	pdf_page *page;
	fz_display_list *list;
	fz_device *dev;
	int start, merge;

	if (showtime)
	{
		start = gettime();
	}

	error = pdf_load_page(&page, xref, pagenum - 1);
	if (error)
		die(fz_rethrow(error, "cannot load page %d in file '%s'", pagenum, filename));

	list = NULL;

	if (uselist)
	{
		list = fz_new_display_list();
		dev = fz_new_list_device(list);
		error = pdf_run_page(xref, page, dev, fz_identity);
		if (error)
			die(fz_rethrow(error, "cannot draw page %d in file '%s'", pagenum, filename));
		fz_free_device(dev);
	}

	if (showxml)
	{
		dev = fz_new_trace_device();
		printf("<page number=\"%d\">\n", pagenum);
		if (list)
			fz_execute_display_list(list, dev, fz_identity, fz_infinite_bbox);
		else
			pdf_run_page(xref, page, dev, fz_identity);
		printf("</page>\n");
		fz_free_device(dev);
	}

	if (showtext)
	{
		fz_text_span *text = fz_new_text_span();
		dev = fz_new_text_device(text);
		if (list)
			fz_execute_display_list(list, dev, fz_identity, fz_infinite_bbox);
		else
			pdf_run_page(xref, page, dev, fz_identity);
		fz_free_device(dev);
		if (showtext > 1) {
			merge = 0;
			if ( showtext > 2 ) {
				merge = 1;
			}
			printf("<page number=\"%d\">\n", pagenum);
			printf("<mediabox x0=\"%f\" y0=\"%f\" x1=\"%f\" y1=\"%f\" \\>\n", page->mediabox.x0,page->mediabox.y0,page->mediabox.x1,page->mediabox.y1);
			fz_debug_text_span_xml(text, merge);
			printf("</page>\n");
		}
		else{
			printf("[Page %d]\n", pagenum);
			fz_debug_text_span(text);
		}
		printf("\n");
		fz_free_text_span(text);
	}

	if (showhtml) {
		fz_text_span *text = fz_new_text_span();
		dev = fz_new_text_device(text);
		if (list)
			fz_execute_display_list(list, dev, fz_identity, fz_infinite_bbox);
		else
			pdf_run_page(xref, page, dev, fz_identity);

		fz_free_device(dev);
		printf("<div id=page_%d style=\"position:absolute; top:0px; left:0px; \">\n", pagenum);
		printf("<img src=\"img%d.png\" \\>\n",pagenum);
		fz_debug_text_span_html(resolution / 72.0,text, &page->mediabox,NULL);
		printf("</div>\n");
		printf("\n");
		fz_free_text_span(text);
	
	}

	if (showjson) {
		fz_text_span *text = fz_new_text_span();
		dev = fz_new_text_device(text);
		if (list)
			fz_execute_display_list(list, dev, fz_identity, fz_infinite_bbox);
		else
			pdf_run_page(xref, page, dev, fz_identity);

		fz_free_device(dev);
		printf("{\"page_number\": %d,\n",pagenum);
		printf("\t\"paragraphs\":\n");
		printf("\t[\n");
		fz_debug_text_span_json(resolution / 72.0, text, &page->mediabox,NULL);
		printf("\t]\n}");
		fz_free_text_span(text);
	}

	if (showmd5 || showtime)
		printf("page %s %d", filename, pagenum);

	if (output || showmd5 || showtime)
	{
		float zoom;
		fz_matrix ctm;
		fz_bbox bbox;
		fz_pixmap *pix;

		zoom = resolution / 72;
		ctm = fz_translate(0, -page->mediabox.y1);
		ctm = fz_concat(ctm, fz_scale(zoom, -zoom));
		ctm = fz_concat(ctm, fz_rotate(page->rotate));
		ctm = fz_concat(ctm, fz_rotate(rotation));
		bbox = fz_round_rect(fz_transform_rect(ctm, page->mediabox));

		/* TODO: banded rendering and multi-page ppm */

		pix = fz_new_pixmap_with_rect(colorspace, bbox);

		if (savealpha)
			fz_clear_pixmap(pix);
		else
			fz_clear_pixmap_with_color(pix, 255);

		dev = fz_new_draw_device(glyphcache, pix);
		if (list)
			fz_execute_display_list(list, dev, ctm, bbox);
		else
			pdf_run_page(xref, page, dev, ctm);
		fz_free_device(dev);

		if (invert)
			fz_invert_pixmap(pix);
		if (gamma_value != 1)
			fz_gamma_pixmap(pix, gamma_value);

		if (output)
		{
			char buf[512];
			sprintf(buf, output, pagenum);
			if (strstr(output, ".pgm") || strstr(output, ".ppm") || strstr(output, ".pnm"))
				fz_write_pnm(pix, buf);
			else if (strstr(output, ".pam"))
				fz_write_pam(pix, buf, savealpha);
			else if (strstr(output, ".png"))
				fz_write_png(pix, buf, savealpha);
			else if (strstr(output, ".pbm")) {
				fz_halftone *ht = fz_get_default_halftone(1);
				fz_bitmap *bit = fz_halftone_pixmap(pix, ht);
				fz_write_pbm(bit, buf);
				fz_drop_bitmap(bit);
				fz_drop_halftone(ht);
			}
		}

		if (showmd5)
		{
			fz_md5 md5;
			unsigned char digest[16];
			int i;

			fz_md5_init(&md5);
			fz_md5_update(&md5, pix->samples, pix->w * pix->h * pix->n);
			fz_md5_final(&md5, digest);

			printf(" ");
			for (i = 0; i < 16; i++)
				printf("%02x", digest[i]);
		}

		fz_drop_pixmap(pix);
	}

	if (list)
		fz_free_display_list(list);

	pdf_free_page(page);

	if (showtime)
	{
		int end = gettime();
		int diff = end - start;

		if (diff < timing.min)
		{
			timing.min = diff;
			timing.minpage = pagenum;
		}
		if (diff > timing.max)
		{
			timing.max = diff;
			timing.maxpage = pagenum;
		}
		timing.total += diff;
		timing.count ++;

		printf(" %dms", diff);
	}

	if (showmd5 || showtime)
		printf("\n");

	pdf_age_store(xref->store, 3);

	fz_flush_warnings();

}

static void countpages(pdf_xref *xref) {
	int spage;
	spage = pdf_count_pages(xref);
	printf("%d", spage);
}

static void drawrange(pdf_xref *xref, char *range)
{
	int page, spage, epage;
	char *spec, *dash;
	int absolute_top=0;

	spec = fz_strsep(&range, ",");
	while (spec)
	{
		dash = strchr(spec, '-');

		if (dash == spec)
			spage = epage = pdf_count_pages(xref);
		else
			spage = epage = atoi(spec);

		if (dash)
		{
			if (strlen(dash) > 1)
				epage = atoi(dash + 1);
			else
				epage = pdf_count_pages(xref);
		}

		spage = CLAMP(spage, 1, pdf_count_pages(xref));
		epage = CLAMP(epage, 1, pdf_count_pages(xref));

		if (spage < epage)
			for (page = spage; page <= epage; page++)
				drawpage(xref, page);
		else
			for (page = spage; page >= epage; page--)
				drawpage(xref, page);

		spec = fz_strsep(&range, ",");
	}
}

int main(int argc, char **argv)
{
	char *password = "";
	int grayscale = 0;
	int accelerate = 1;
	pdf_xref *xref;
	fz_error error;
	int c;

	while ((c = fz_getopt(argc, argv, "o:p:r:j:R:Aab:dgmthJxn5G:I")) != -1)
	{
		switch (c)
		{
		case 'o': output = fz_optarg; break;
		case 'p': password = fz_optarg; break;
		case 'r': resolution = atof(fz_optarg); break;
		case 'R': rotation = atof(fz_optarg); break;
		case 'A': accelerate = 0; break;
		case 'a': savealpha = 1; break;
		case 'b': alphabits = atoi(fz_optarg); break;
		case 'm': showtime++; break;
		case 't': showtext++; break;
		case 'h': showhtml++; break;
		case 'J': showjson++; break;
		case 'x': showxml++; break;
		case 'n': showpages++; break;
		case '5': showmd5++; break;
		case 'g': grayscale++; break;
		case 'd': uselist = 0; break;
		case 'G': gamma_value = atof(fz_optarg); break;
		case 'I': invert++; break;
		default: usage(); break;
		}
	}

	fz_set_aa_level(alphabits);

	if (fz_optind == argc)
		usage();

	if (!showpages && !showtext && !showxml && !showhtml && !showjson && !showtime && !showmd5 && !output)
	{
		printf("nothing to do\n");
		exit(0);
	}

	if(showpages) {
		filename = argv[fz_optind++];
		error = pdf_open_xref(&xref, filename, password);
		if (error) {
			die(fz_rethrow(error, "cannot open document: %s", filename));
		}
		error = pdf_load_page_tree(xref);
		if (error) {
			die(fz_rethrow(error, "cannot load page tree: %s", filename));
		}
		countpages(xref);
		pdf_free_xref(xref);
		exit(0);
	}

	if (accelerate)
		fz_accelerate();

	glyphcache = fz_new_glyph_cache();

	colorspace = fz_device_rgb;
	if (grayscale)
		colorspace = fz_device_gray;
	if (output && strstr(output, ".pgm"))
		colorspace = fz_device_gray;
	if (output && strstr(output, ".ppm"))
		colorspace = fz_device_rgb;
	if (output && strstr(output, ".pbm"))
		colorspace = fz_device_gray;

	timing.count = 0;
	timing.total = 0;
	timing.min = 1 << 30;
	timing.max = 0;
	timing.minpage = 0;
	timing.maxpage = 0;

	if ((showxml || showtext > 1) && !showpages)
		printf("<?xml version=\"1.0\"?>\n");
	while (fz_optind < argc)
	{
		filename = argv[fz_optind++];

		error = pdf_open_xref(&xref, filename, password);
		if (error)
			die(fz_rethrow(error, "cannot open document: %s", filename));

		error = pdf_load_page_tree(xref);
		if (error)
			die(fz_rethrow(error, "cannot load page tree: %s", filename));

		if ((showxml || showtext > 1) && !showpages)
			printf("<document name=\"%s\">\n", filename);
		if (showhtml)
			printf("<html>\n<head>\n<title>%s</title>\n</head>\n<body>",filename);

		if (fz_optind == argc || !isrange(argv[fz_optind]))
			drawrange(xref, "1-");
		if (fz_optind < argc && isrange(argv[fz_optind]))
			drawrange(xref, argv[fz_optind++]);

		if (showxml || showtext > 1)
			printf("</document>\n");
		if (showhtml)
			printf("</body></html>",filename);

		pdf_free_xref(xref);
	}

	if (showtime && !showpages)
	{
		printf("total %dms / %d pages for an average of %dms\n",
			timing.total, timing.count, timing.total / timing.count);
		printf("fastest page %d: %dms\n", timing.minpage, timing.min);
		printf("slowest page %d: %dms\n", timing.maxpage, timing.max);
	}

	fz_free_glyph_cache(glyphcache);

	fz_flush_warnings();

	return 0;
}
