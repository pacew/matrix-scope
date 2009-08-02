/* sigma &#931; */
#include <opus/opus.h>
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <pango/pangocairo.h>

GtkWidget *window;
GdkPixbuf *pixbuf;

#define RTOD(x) ((x) / 2.0 / M_PI * 360)
#define DTOR(x) ((x) / 360.0 * 2 * M_PI)

double mouse_x, mouse_y, mouse_ang;

int running;
int recompute;

cairo_t *cr;

PangoLayout *p_layout;
PangoFontDescription *font_description;

struct numbox {
	struct numbox *next;
	double x, y;
	double *valp;
};

struct numbox *numboxes;
struct numbox *highlighted_numbox;

struct numbox *
get_numbox (double *valp)
{
	struct numbox *np;

	for (np = numboxes; np; np = np->next) {
		if (np->valp == valp)
			return (np);
	}

	np = xcalloc (1, sizeof *np);
	np->valp = valp;

	np->next = numboxes;
	numboxes = np;
	return (np);
}
	
void
put_numbox (double *valp, double x, double y)
{
	struct numbox *np;

	np = get_numbox (valp);
	np->x = x;
	np->y = y;
}

#define MATRIX_A "A"
#define MATRIX_S "S"
#define MATRIX_LAMBDA "&#923;"
#define MATRIX_Sinv "S<sup>-1</sup>"
#define MATRIX_U "U"
#define MATRIX_SIGMA "&#931;"
#define MATRIX_Vtrans "V<sup>T</sup>"

struct matrix {
	struct matrix *next;
	char name[100];
	double a, b, c, d;
	double x, y;
};

struct matrix *matricies;

struct matrix *
get_matrix (char *name)
{
	struct matrix *mp;

	for (mp = matricies; mp; mp = mp->next) {
		if (strcmp (mp->name, name) == 0)
			return (mp);
	}

	mp = xcalloc (1, sizeof *mp);
	xstrcpy (mp->name, name, sizeof mp->name);

	mp->next = matricies;
	matricies = mp;
	return (mp);
}

double bx, by;

double
get_secs (void)
{
	struct timeval tv;
	static double start;
	double now;

	gettimeofday (&tv, NULL);
	now = tv.tv_sec + tv.tv_usec / 1e6;
	if (start == 0) {
		start = now;
		usleep (1000);
		gettimeofday (&tv, NULL);
		now = tv.tv_sec + tv.tv_usec / 1e6;
	}
	return (now - start);
}

void
usage (void)
{
	fprintf (stderr, "usage: svd\n");
	exit (1);
}

int width, height;
int last_width, last_height;

double graph_width, graph_height;
double graph_cx, graph_cy;

double input_x_scale, input_x_offset;
double input_y_scale, input_y_offset;

double output_x_scale, output_x_offset;
double output_y_scale, output_y_offset;

int font_height_pixels;

double numbox_width, numbox_height;
double space_width, line_height;

double numbox_space;

void
layout_matrix (char *name, double start_x, double start_y)
{
	double x, y;
	struct matrix *mp;

	mp = get_matrix (name);

	x = start_x;
	y = start_y;

	mp->x = x;
	mp->y = y;

	y += line_height * .6;

	put_numbox (&mp->a, x, y);
	x += numbox_width + numbox_space;
	put_numbox (&mp->b, x, y);

	x = start_x;
	y += line_height;

	put_numbox (&mp->c, x, y);
	x += numbox_width + numbox_space;
	put_numbox (&mp->d, x, y);
}

double matrix_width;

double major_axis, minor_axis;

void compute (double x, double y);

void
find_matrix_extents (void)
{
	double ang, x, y;
	double dist;

	major_axis = 0;
	minor_axis = 0;
	for (ang = 0; ang < 2 * M_PI; ang += M_PI/10) {
		x = cos (ang);
		y = sin (ang);
		compute (x, y);

		dist = hypot (bx, by);

		if (ang == 0) {
			major_axis = dist;
			minor_axis = dist;
		} else {
			if (dist > major_axis)
				major_axis = dist;
			if (dist < minor_axis)
				minor_axis = dist;
		}
	}

	printf ("axes %g %g\n", major_axis, minor_axis);
}

double graph_size;

void
layout (void)
{
	double left, x, y;
	PangoRectangle ink_rect;
	PangoRectangle logical_rect;
	char text[1000];

	if (last_width == width && last_height == height)
		return;

	find_matrix_extents ();

	last_width = width;
	last_height = height;

	graph_width = width;
	graph_height = height;

	graph_cx = graph_width / 2;
	graph_cy = graph_height / 2;

	graph_size = graph_width;
	if (graph_height < graph_size)
		graph_size = graph_height;

	input_x_offset = rint (graph_width / 2.0);
	input_y_offset = rint (graph_height / 2.0);

	output_x_offset = rint (graph_width / 2.0);
	output_y_offset = rint (graph_height / 2.0);

	input_x_scale = graph_width / major_axis / 4;
	input_y_scale = -1 * input_x_scale;
	output_x_scale = input_x_scale;
	output_y_scale = -1 * input_x_scale;

	font_height_pixels = rint (height / 45.0);

	if (font_description)
		pango_font_description_free (font_description);

	font_description = pango_font_description_new ();
	pango_font_description_set_family
		(font_description, "courier");
	pango_font_description_set_weight
		(font_description, PANGO_WEIGHT_NORMAL);
	pango_font_description_set_size
		(font_description, font_height_pixels * PANGO_SCALE);

	sprintf (text, "%6.2f", -23.45);
	pango_layout_set_text (p_layout, text, -1);
	pango_layout_get_extents (p_layout, &ink_rect, &logical_rect);

	numbox_width = rint ((double)logical_rect.width / PANGO_SCALE);
	numbox_height = rint ((double)logical_rect.height / PANGO_SCALE);

	line_height = numbox_height * 1.2;

	sprintf (text, " ");
	pango_layout_set_text (p_layout, text, -1);
	pango_layout_get_extents (p_layout, &ink_rect, &logical_rect);
	space_width = rint ((double)logical_rect.width / PANGO_SCALE);

	numbox_space = space_width * 2;

	matrix_width = 2 * numbox_width + 3 * numbox_space;

	left = space_width * 2;

	x = left;
	y = line_height / 2;

	layout_matrix (MATRIX_A, x, y);

	left = width - 3 * matrix_width;

	x = left;
	y = line_height / 2;
	layout_matrix (MATRIX_S, x, y);
	x += matrix_width;
	layout_matrix (MATRIX_LAMBDA, x, y);
	x += matrix_width;
	layout_matrix (MATRIX_Sinv, x, y);


	x = left;
	y = height - 3 * line_height;
	layout_matrix (MATRIX_U, x, y);
	x += matrix_width;
	layout_matrix (MATRIX_SIGMA, x, y);
	x += matrix_width;
	layout_matrix (MATRIX_Vtrans, x, y);

}


double
inputx (double x)
{
	return (x * input_x_scale + input_x_offset);
}

double
inputy (double x)
{
	return (x * input_y_scale + input_y_offset);
}

double
outputx (double x)
{
	return (x * output_x_scale + output_x_offset);
}

double
outputy (double x)
{
	return (x * output_y_scale + output_y_offset);
}

void
compute (double x, double y)
{
	struct matrix *mp;

	mp = get_matrix (MATRIX_A);

	bx = mp->a * x + mp->b * y;
	by = mp->c * x + mp->d * y;
}

#define idx(r,c) ((c)*2+(r))

void dgeev_(char *jobvl, char *jobvr,
	    int *n,
	    double *a, int *lda,
	    double *wr, double *wi,
	    double *vl, int *ldvl,
	    double *vr, int *ldvr,
	    double *work, int *lwork,
	    int *info);

void dgetri_(int *n, double *a, int *lda, int *ipiv, 
	     double *work, int *lwork, int *info);

void
dswap (double *a, double *b)
{
	double t;
	t = *a;
	*a = *b;
	*b = t;
}

void
factor (void)
{
	double A[4];
	struct matrix *mp, *s, *lam, *sinv;
	char *jobvl, *jobvr;
	int N;
	double work[1000];
	int work_size;
	int info;
	double eigenvals_re[2], eigenvals_im[2];
	double eigenvecs_right[4];
	double eigenvecs_left[4];
	double det;

	mp = get_matrix (MATRIX_A);

	A[idx(0,0)] = mp->a;
	A[idx(0,1)] = mp->b;
	A[idx(1,0)] = mp->c;
	A[idx(1,1)] = mp->d;

	jobvl = "V";
	jobvr = "V";
	N = 2;
	work_size = sizeof work / sizeof work[0];
	info = 0;

	dgeev_(jobvl, /* jobvl */
	       jobvr,  /* jobvr */
	       &N, /* N */
	       A, &N, /* A LDA array and leading dimension of A */
	       eigenvals_re,
	       eigenvals_im,
	       eigenvecs_left, &N,
	       eigenvecs_right, &N,
	       work, &work_size,
	       &info);

	if (info < 0)
		fatal ("sgeev_: bad arg %d\n", -info);

	lam = get_matrix (MATRIX_LAMBDA);
	s = get_matrix (MATRIX_S);
	sinv = get_matrix (MATRIX_Sinv);

	if (info == 1) {
		s->a = s->b = s->c = s->d = atof ("nan");
		sinv->a = sinv->b = sinv->c = sinv->d = atof ("nan");
		
		lam->a = eigenvals_re[0];
		lam->b = atof ("nan");
		lam->c = atof ("nan");
		lam->d = atof ("nan");
		return;
	}

	if (eigenvals_im[0] != 0) {
		s->a = s->b = s->c = s->d = atof ("nan");
		sinv->a = sinv->b = sinv->c = sinv->d = atof ("nan");
		
		lam->a = atof ("nan");
		lam->b = atof ("nan");
		lam->c = atof ("nan");
		lam->d = atof ("nan");
		return;
	}

	lam->a = eigenvals_re[0];
	lam->b = 0;
	lam->c = 0;
	lam->d = eigenvals_re[1];

	s->a = eigenvecs_right[idx(0,0)];
	s->b = eigenvecs_right[idx(0,1)];
	s->c = eigenvecs_right[idx(1,0)];
	s->d = eigenvecs_right[idx(1,1)];

	if (fabs (lam->a) < fabs (lam->d)) {
		dswap (&lam->a, &lam->d);
		dswap (&s->a, &s->b);
		dswap (&s->c, &s->d);
	}

	det = s->a * s->d - s->b * s->c;
	sinv->a = -1 * s->d / (-1 * det);
	sinv->b =      s->b / (-1 * det);
	sinv->c =      s->c / (-1 * det);
	sinv->d = -1 * s->a / (-1 * det);
}

cairo_surface_t *surface;
GdkPixmap *pixmap;
GdkGC *gc;

cairo_t *cr_pixmap;

void
draw_matrix (struct matrix *mp)
{
	double x, y;
	PangoRectangle ink_rect;
	PangoRectangle logical_rect;

	cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

	pango_layout_set_markup (p_layout, mp->name, strlen (mp->name));

	pango_layout_get_extents (p_layout, &ink_rect, &logical_rect);

	x = mp->x
		+ (numbox_width + numbox_space + numbox_width) / 2
		+ (double)logical_rect.x / PANGO_SCALE / 2
		+ (double)logical_rect.width / PANGO_SCALE / 2;
	y = mp->y
		+ (double)logical_rect.y / PANGO_SCALE
		- (double)logical_rect.height / PANGO_SCALE
		+ numbox_height * .5;

	cairo_move_to (cr, x, y);
	pango_cairo_show_layout (cr, p_layout);
}

#define PERIOD 4.0

double sim_time;

struct seg {
	double ang;
	double x_in, y_in;
	double x_out, y_out;
};
#define NSEGS 100
struct seg segs[NSEGS];


void
color_axis (void)
{
	double v = .8;
	cairo_set_source_rgb (cr, v, v, v);
}

void
color_data_background (void)
{
	double v = .7;
	cairo_set_source_rgb (cr, v, v, v);
}

void
color_hands (void)
{
	cairo_set_source_rgb (cr, 1, 0, 0);
}

void
color_ev1 (void)
{
	cairo_set_source_rgb (cr, .6, .6, .8);
}

void
color_ev1_highlight (void)
{
	cairo_set_source_rgb (cr, 0, 0, 1);
}

void
color_ev2 (void)
{
	cairo_set_source_rgb (cr, .6, .8, .6);
}

void
color_ev2_highlight (void)
{
	cairo_set_source_rgb (cr, 0, 1, 0);
}

double scale;

double
xscale (double x)
{
	return (scale * x + graph_cx);
}

double
yscale (double y)
{
	return (-1 * scale * y + graph_cy);
}

int
samedir (double x1, double y1, double x2, double y2)
{
	double ang, d;

	d = hypot (x1, y1);
	if (fabs (d) < .1)
		return (0);
	x1 /= d;
	y1 /= d;
	
	d = hypot (x2, y2);
	if (fabs (d) < .1)
		return (0);
	x2 /= d;
	y2 /= d;

	ang = acos (x1 * x2 + y1 * y2);
	if (ang > M_PI/2)
		ang = M_PI - ang;

	if (fabs (ang) < DTOR (10))
		return (1);

	return (0);
}

void
redraw (void)
{
	int i;
	struct numbox *np;
	char text[1000];
	struct matrix *mp;
	double maxx, maxy;
	struct seg *sp;
	double ang, x_ang;
	double x, y;
	double x_in, y_in, x_out, y_out;
	struct matrix *s;
	double x1, y1, x2, y2;
	int highlight;
	double val;
	
	if (recompute) {
		recompute = 0;
		factor ();
	}

	width = window->allocation.width;
	height = window->allocation.height;

	cr = gdk_cairo_create (GDK_WINDOW (window->window));
	p_layout = pango_cairo_create_layout (cr);
	pango_layout_set_font_description (p_layout, font_description);

	x_ang = fmod (get_secs () * M_PI * 2 / PERIOD, 2 * M_PI);
	/* 0 <= x_ang <= 2 * M_PI */
		
	x_in = cos (x_ang);
	y_in = sin (x_ang);

	layout ();

	maxx = 0;
	maxy = 0;
	for (i = 0; i < NSEGS; i++) {
		sp = &segs[i];
		ang = (double)i / NSEGS * 2 * M_PI;
		sp->ang = ang;
		sp->x_in = cos (ang);
		sp->y_in = sin (ang);
		compute (sp->x_in, sp->y_in);
		sp->x_out = bx;
		sp->y_out = by;
		
		if (i == 0) {
			maxx = sp->x_out;
			maxy = sp->y_out;
		} else {
			if (sp->x_out > maxx)
				maxx = sp->x_out;
			if (sp->y_out > maxy)
				maxy = sp->y_out;
		}
	}
		
	scale = 50;

	color_axis ();
	cairo_move_to (cr, graph_cx, 0);
	cairo_line_to (cr, graph_cx, graph_height);
	cairo_stroke (cr);
	cairo_move_to (cr, 0, graph_cy);
	cairo_line_to (cr, graph_width, graph_cy);
	cairo_stroke (cr);

	s = get_matrix ("S");
	if (! isnan (s->a) && ! isnan (s->c)) {
		highlight = 0;

		if (samedir (s->a, s->c, x_in, y_in))
			highlight = 1;

		if (highlight)
			color_ev1_highlight ();
		else
			color_ev1 ();

		x1 = 2 * scale * s->a;
		y1 = 2 * scale * s->c;
		x2 = -2 * scale * s->a;
		y2 = -2 * scale * s->c;

		cairo_move_to (cr, xscale (x1), yscale (y1));
		cairo_line_to (cr, xscale (x2), yscale (y2));
		cairo_stroke (cr);
	}

	if (! isnan (s->b) && ! isnan (s->d)) {
		highlight = 0;

		if (samedir (s->b, s->d, x_in, y_in))
			highlight = 1;

		if (highlight)
			color_ev2_highlight ();
		else
			color_ev2 ();

		x1 = 2 * scale * s->b;
		y1 = 2 * scale * s->d;
		x2 = -2 * scale * s->b;
		y2 = -2 * scale * s->d;

		cairo_move_to (cr, xscale (x1), yscale (y1));
		cairo_line_to (cr, xscale (x2), yscale (y2));
		cairo_stroke (cr);
	}


	color_data_background ();
	for (i = 0; i < NSEGS; i++) {
		sp = &segs[i];
		x = scale * sp->x_in + graph_cx;
		y = -1 * scale * sp->y_in + graph_cy;
		if (i == 0) {
			cairo_move_to (cr, x, y);
		} else {
			cairo_line_to (cr, x, y);
		}
	}
	cairo_close_path (cr);
	cairo_stroke (cr);

	for (i = 0; i < NSEGS; i++) {
		sp = &segs[i];
		x = scale * sp->x_out + graph_cx;
		y = -1 * scale * sp->y_out + graph_cy;
		if (i == 0) {
			cairo_move_to (cr, x, y);
		} else {
			cairo_line_to (cr, x, y);
		}
	}
	cairo_close_path (cr);
	cairo_stroke (cr);

	color_hands ();

	compute (x_in, y_in);
	x_out = bx;
	y_out = by;

	cairo_move_to (cr, graph_cx, graph_cy);
	x = scale * x_in + graph_cx;
	y = -1 * scale * y_in + graph_cy;
	cairo_line_to (cr, x, y);
	cairo_stroke (cr);
	
	cairo_move_to (cr, graph_cx, graph_cy);
	x = scale * x_out + graph_cx;
	y = -1 * scale * y_out + graph_cy;
	cairo_line_to (cr, x, y);
	cairo_stroke (cr);


	for (mp = matricies; mp; mp = mp->next)
		draw_matrix (mp);

	for (np = numboxes; np; np = np->next) {
		if (np == highlighted_numbox)
			cairo_set_source_rgb (cr, .8, .8, 1);
		else
			cairo_set_source_rgb (cr, .8, .8, .8);

		cairo_rectangle (cr, np->x, np->y,
				 numbox_width + numbox_space,
				 numbox_height);
		cairo_fill (cr);

		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);

		val = *np->valp;
		if (val <= -100)
			sprintf (text, "%6s", "-***");
		else if (val > 100)
			sprintf (text, "%6s", "+***");
		else
			sprintf (text, "%6.2f", *np->valp);
		pango_layout_set_text (p_layout, text, -1);
		cairo_move_to (cr, np->x, np->y);
		pango_cairo_show_layout (cr, p_layout);
	}

	g_object_unref (p_layout);

	cairo_destroy (cr);
}

gboolean
tick (gpointer data)
{
	if (running)
		gtk_widget_queue_draw_area (window, 0, 0, width, height);
	return (1);
}

gboolean
expose_event (GtkWidget *widget, GdkEventExpose *ev, gpointer user_data)
{
	static int beenhere;

	if (beenhere == 0) {
		beenhere = 1;
		g_timeout_add (30, tick, NULL);
	}

	redraw ();

	/* also gdk_display_flush */
	gdk_display_sync (gtk_widget_get_display (window));
	return (TRUE);
}

gboolean
button_press_event (GtkWidget *widget, GdkEventButton *ev, gpointer user_data)
{
	/* GDK_CONTROL_MASK GDK_SHIFT_MASK */
	printf ("button press %d %g %g 0x%x\n",
		ev->button, ev->x, ev->y, ev->state);
	return (TRUE);
}

void
incr_val (int dir, int state)
{
	double incr;
	struct numbox *np;

	if (state & GDK_CONTROL_MASK)
		incr = 1;
	else if (state & GDK_SHIFT_MASK)
		incr = 100;
	else
		incr = 10;

	incr *= dir;

	if ((np = highlighted_numbox) != NULL) {
		*np->valp = rint (*np->valp * 100 + incr) / 100.0;
		recompute = 1;
	}
}


gboolean
scroll_event (GtkWidget *widget, GdkEventScroll *ev, gpointer user_data)
{
	switch (ev->direction) {
	case GDK_SCROLL_UP:
		incr_val (1, ev->state);
		break;
	case GDK_SCROLL_DOWN:
		incr_val (-1, ev->state);
		break;
	default:
		break;
	}
	
	return (TRUE);
}

gboolean
key_press_event (GtkWidget *widget, GdkEventKey *ev, gpointer user_data)
{
	switch (ev->keyval) {
	case GDK_Up:
		incr_val (1, ev->state);
		break;
	case GDK_Down:
		incr_val (-1, ev->state);
		break;

	case ' ':
		running ^= 1;
		break;

	case 'q':
	case 'c':
	case 'w':
		exit (0);
	}
	return (TRUE);
}

void
process_motion (int x, int y)
{
	struct numbox *np;

	mouse_x = (x - graph_cx) / scale;
	mouse_y = ((height - y) - graph_cy) / scale;
	mouse_ang = atan2 (mouse_y, mouse_x);

	highlighted_numbox = NULL;
	for (np = numboxes; np; np = np->next) {
		if (np->x <= x && x <= np->x + numbox_width
		    && np->y <= y && y <= np->y + numbox_height) {
			highlighted_numbox = np;
			break;
		}
	}
}


gboolean
motion_notify_event (GtkWidget *widget, GdkEventMotion *ev, gpointer user_data)
{
	int x, y;
	static int last_x, last_y;

	x = ev->x;
	y = ev->y;

	if (x != last_x || y != last_y) {
		last_x = x;
		last_y = y;
		process_motion (x, y);
	}

	gdk_event_request_motions (ev);

	return (TRUE);
}


int
main (int argc, char **argv)
{
	struct matrix *mp;

	gtk_init (&argc, &argv);

	if (optind != argc)
		usage ();

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), "matrix-scope");

	g_signal_connect (G_OBJECT (window), "delete_event",
			  G_CALLBACK (gtk_main_quit), NULL);

	g_signal_connect (G_OBJECT (window), "destroy",
			  G_CALLBACK (gtk_main_quit), NULL);

	g_signal_connect (G_OBJECT (window), "expose_event",
			  G_CALLBACK (expose_event), NULL);

	g_signal_connect (G_OBJECT (window), "key_press_event",
			  G_CALLBACK (key_press_event), NULL);

	g_signal_connect (G_OBJECT (window), "button_press_event",
			  G_CALLBACK (button_press_event), NULL);

	g_signal_connect (G_OBJECT (window), "scroll_event",
			  G_CALLBACK (scroll_event), NULL);

	g_signal_connect (G_OBJECT (window), "motion_notify_event",
			  G_CALLBACK (motion_notify_event), NULL);

	gtk_widget_add_events (window,
			       GDK_EXPOSURE_MASK
			       | GDK_POINTER_MOTION_MASK
			       | GDK_POINTER_MOTION_HINT_MASK
			       | GDK_BUTTON_PRESS_MASK
			       | GDK_KEY_PRESS_MASK
			       | GDK_SCROLL_MASK);

	gtk_window_set_default_size (GTK_WINDOW(window), 640, 480);

	gtk_widget_show (window);

	running = 1;

	mp = get_matrix (MATRIX_A);
	mp->a = 2;
	mp->b = 0;
	mp->c = 1;
	mp->d = 3;

	/* 2 : (.71,-.71)   3 : (0, -1) */
	/* rotation [.866, .5; -.5, .866] ev 0 : (.54,-.84), 0 : (.54,-.84) */
	/* neg ev: [-1, 1; 2,3 ] ev -1.45 : (-.91,.41), 3.45:(-.22,-.98) */
	/* sing [1,2;2,4] ev 0 : (-.89,.45); 5 : (-.45, -.89) */
	/* defective [2,1;0,2] ev 2 : (-1, 0); 2 : (none) */
	/* sym [3,1;1,3] ev 2 : (-.71,.71); 4 : (-.71,-.71) */
	/* octave: [evect, eval] = eig(a) */

	factor ();

	gtk_main ();

	return (0);
}
