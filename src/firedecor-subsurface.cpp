#include <glm/gtc/matrix_transform.hpp>

#include <linux/input-event-codes.h>

#include <wayfire/nonstd/wlroots.hpp>
#include <wayfire/compositor-surface.hpp>
#include <wayfire/output.hpp>
#include <wayfire/opengl.hpp>
#include <wayfire/core.hpp>
#include <wayfire/decorator.hpp>
#include <wayfire/view-transform.hpp>
#include <wayfire/signal-definitions.hpp>

#include "firedecor-layout.hpp"
#include "firedecor-theme.hpp"

#include "cairo-simpler.hpp"

#include <wayfire/plugins/common/cairo-util.hpp>

//#include <cairo.h>

#define INACTIVE 0
#define ACTIVE 1
#define FORCE true
#define DONT_FORCE false

#include <fstream>

class simple_decoration_surface : public wf::surface_interface_t,
	public wf::compositor_surface_t {
	bool _mapped = true;
    wayfire_view view;

    wf::signal_connection_t title_set = [=] (wf::signal_data_t *data) {
        if (get_signaled_view(data) == view) {
            update_layout(FORCE);
            view->damage(); // trigger re-render
        }
    };

    void update_title(double scale) {
		cairo_surface_t *surface;
		wf::dimensions_t title_size = {
    		(int)(title.dims.width * scale), (int)(title.dims.height * scale)
        };

		auto o = wf::firedecor::HORIZONTAL;
        surface = theme.form_title(title.text, title_size, ACTIVE, o);
        cairo_surface_upload_to_texture(surface, title.hor[ACTIVE]);
        surface = theme.form_title(title.text, title_size, INACTIVE, o);
        cairo_surface_upload_to_texture(surface, title.hor[INACTIVE]);
		o = wf::firedecor::VERTICAL;
		surface = theme.form_title(title.text, title_size, ACTIVE, o);
        cairo_surface_upload_to_texture(surface, title.ver[ACTIVE]);
        surface = theme.form_title(title.text, title_size, INACTIVE, o);
        cairo_surface_upload_to_texture(surface, title.ver[INACTIVE]);
        cairo_surface_destroy(surface); 

        title_needs_update = false;
    }

    void update_icon() {
        if (view->get_app_id() != icon.app_id) {
	        icon.app_id = view->get_app_id();
	        auto surface = theme.form_icon(icon.app_id);
	        cairo_surface_upload_to_texture(surface, icon.texture);
            cairo_surface_destroy(surface);
        }
    }

    void update_layout(bool force) {
        auto title_colors = theme.get_title_colors();

        if ((title.colors != title_colors) || force) {
            /* Updating the cached variables */
            title.colors = title_colors;
	        title.text = (theme.get_debug_mode()) ? view->get_app_id() :
	        										view->get_title();

	        title.dims = theme.get_text_size(title.text, size.width);

		    title_needs_update = true;

            /* Necessary in order to immediately place areas correctly */
    		layout.resize(size.width, size.height, title.dims);
        }

    }

    /** Title variables */
    struct {
        wf::simple_texture_t hor[2];
        wf::simple_texture_t ver[2];
        std::string text = "";
        wf::firedecor::color_set_t colors;
        wf::dimensions_t dims;
    } title;

    bool title_needs_update = false;

    /** Icon variables */
    struct {
	    wf::simple_texture_t texture;
	    std::string app_id = "";
    } icon;

    /** Corner variables */
    struct corner_texture_t {
	    wf::simple_texture_t tex[2];
	    cairo_surface_t *surf[2];
	    wf::geometry_t g_rel;
	    int r;
    };

    struct {
        corner_texture_t tr, tl, bl, br;
    } corners;

    /** Edge variables */
    struct edge_colors_t {
	    wf::firedecor::color_set_t border, outline;
    } edges;

    /** Accent variables */
    struct accent_texture_t {
        wf::simple_texture_t tr[2];
        wf::simple_texture_t tl[2];
        wf::simple_texture_t bl[2];
        wf::simple_texture_t br[2];
        int radius;
    };

    std::vector<accent_texture_t> accent_textures;

    /** Other general variables */
    wf::firedecor::decoration_theme_t theme;
    wf::firedecor::decoration_layout_t layout;
    wf::region_t cached_region;
    wf::dimensions_t size;

	void update_corners(edge_colors_t colors, int corner_radius, double scale) {
		if ((this->corner_radius != corner_radius) ||
			(edges.border != colors.border) ||
			(edges.outline != colors.outline)) {
    		corners.tr.r = corners.tl.r = corners.bl.r = corners.br.r = 0;

    		std::stringstream round_on_str(theme.get_round_on());
    		std::string corner;
    		while (round_on_str >> corner) {
        		if (corner == "all") {
            		corners.tr.r = corners.tl.r = corners.bl.r = corners.br.r
        		                 = theme.get_corner_radius() * scale;
            		break;
        		} else if (corner == "tr") {
            		corners.tr.r = (theme.get_corner_radius() * scale);
        		} else if (corner == "tl") {
            		corners.tl.r = (theme.get_corner_radius() * scale);
        		} else if (corner == "bl") {
            		corners.bl.r = (theme.get_corner_radius() * scale);
        		} else if (corner == "br") {
            		corners.br.r = (theme.get_corner_radius() * scale);
        		}
    		}
    		int height = std::max( { corner_radius, border_size.top,
    		                         border_size.bottom });
    		auto create_s_and_t = [&](corner_texture_t& t, double angle) {
        		for (auto a : { ACTIVE, INACTIVE }) {
            		t.surf[a] = theme.form_corner(ACTIVE, scale, angle, height);
        			cairo_surface_upload_to_texture(t.surf[a], t.tex[a]);
        		}
    		};
    		create_s_and_t(corners.tr, 0);
    		create_s_and_t(corners.tl, M_PI / 2);
    		create_s_and_t(corners.bl, M_PI);
    		create_s_and_t(corners.br, 3 * M_PI / 2);

    		corners.tr.g_rel = { size.width - corner_radius, 0,
    		                     corner_radius, height };
    		corners.tl.g_rel = { 0, 0, corner_radius, height };
    		corners.bl.g_rel = { 0, size.height - height, corner_radius, height };
    		corners.br.g_rel = { size.width - corner_radius, size.height - height,
    		                     corner_radius, height };

			edges.border.active    = colors.border.active;
			edges.border.inactive  = colors.border.inactive;
			edges.outline.active   = colors.outline.active;
			edges.outline.inactive = colors.outline.inactive;
			this->corner_radius    = corner_radius;
		}
	}

  public:
    wf::firedecor::border_size_t border_size;
    int corner_radius;

    simple_decoration_surface(wayfire_view view, wf::firedecor::theme_options options)
      : theme{options}, 
    	layout{theme, [=] (wlr_box box) {this->damage_surface_box(box); }} {
        this->view = view;
        view->connect_signal("title-changed", &title_set);

        // make sure to hide frame if the view is fullscreen
        update_decoration_size();
    }
    
    virtual bool is_mapped() const final {
        return _mapped;
    }

    wf::point_t get_offset() final {
        return { -border_size.left, -border_size.top };
    }

    virtual wf::dimensions_t get_size() const final {
        return size;
    }

    void render_title(const wf::framebuffer_t& fb, wf::geometry_t geometry,
                      wf::firedecor::edge_t edge, wf::geometry_t scissor) {
	    if (title_needs_update) {
    	    update_title(fb.scale);
	    }

	    wf::simple_texture_t *texture;
	    uint32_t bits = 0;
	    if (edge == wf::firedecor::EDGE_TOP || edge == wf::firedecor::EDGE_BOTTOM) {
	        bits = OpenGL::TEXTURE_TRANSFORM_INVERT_Y;
	        texture = &title.hor[view->activated];
	    } else {
    	    texture = &title.ver[view->activated];
	    }
		OpenGL::render_begin(fb);
        fb.logic_scissor(scissor);
        OpenGL::render_texture(texture->tex, fb, geometry, glm::vec4(1.0f), bits);
		OpenGL::render_end();
    }

    void render_icon(const wf::framebuffer_t& fb, wf::geometry_t g,
                     const wf::geometry_t& scissor, int32_t bits) {
        update_icon();
		OpenGL::render_begin(fb);
		fb.logic_scissor(scissor);
		OpenGL::render_texture(icon.texture.tex, fb, g, glm::vec4(1.0f), bits);
		OpenGL::render_end();
    }

    wf::color_t alpha_transform(wf::color_t c) {
	    return { c.r * c.a, c.g * c.a, c.b * c.a, c.a };
    }

    void form_accent_corners(int r, wf::geometry_t accent, std::string round) {
        const auto format = CAIRO_FORMAT_ARGB32;
        cairo_surface_t *surfaces[8];
        double angle = 0;
        /** Colors of the accent and background, respectively */
        wf::color_t a_color;
        wf::color_t b_color;
        wf::point_t p[] = { { 0, 0 }, { r, 0 }, { r, r }, { 0, r } };

        int h = std::max({ corner_radius, border_size.top, border_size.bottom });
        wf::geometry_t a_corners[] = {
            { accent.x + accent.width - r, accent.y, r, r },
            { accent.x, accent.y, r, r },
            { accent.x, accent.y + accent.height - r, r, r },
            { accent.x + accent.width - r, accent.y + accent.height - r, r, r }
        };

        wf::geometry_t cuts[] = { 
           { accent.x + r, accent.y,
             accent.width - 2 * r, accent.height },
           { accent.x, accent.y + r, r, accent.height - 2 * r },
           { accent.x + accent.width - r, accent.y + r, r,
             accent.height - 2 * r }
        };

        std::string to_round[] = { "tr", "tl", "bl", "br" };

        for (int i = 0, j = 0; i < 8; i++, angle += M_PI / 2, j = i % 4) {
            if (i < 4) {
                a_color = theme.get_accent_colors().inactive;
                b_color = theme.get_border_colors().inactive;
            } else {
                a_color = theme.get_accent_colors().active;
                b_color = theme.get_border_colors().active;
            }
                
            surfaces[i] = cairo_image_surface_create(format, r, r);
            auto cr_a = cairo_create(surfaces[i]);

            /** Background rectangle, behind the accent's corner */
            cairo_set_source_rgba(cr_a, b_color);
            cairo_rectangle(cr_a, 0, 0, r, r);
            cairo_fill(cr_a);

            bool do_round = (round.find(to_round[j]) != std::string::npos ||
                             round == "a");

            /** Dealing with intersection between the view's corners and accent */
            for (auto *c : { &corners.tr, &corners.tl, &corners.bl, &corners.br } ) {

                /** Skip everything, if the areas don't intersect */
                auto in = wf::geometry_intersection(a_corners[j], c->g_rel);
                if (in.width == 0 || in.height == 0) { continue; }

                /**** Rectangle to cut the background from the accent's corner */
                /* View's corner position relative to the accent's corner */
                wf::point_t v_rel_a = { 
                    c->g_rel.x - a_corners[j].x, c->g_rel.y - a_corners[j].y
                };

                cairo_set_operator(cr_a, CAIRO_OPERATOR_CLEAR);
                cairo_rectangle(cr_a, v_rel_a, corner_radius, h);
                cairo_fill(cr_a);
                /****/

                /** Removal of intersecting areas from the view corner */
                for (auto active : { ACTIVE, INACTIVE }) {

                    /** Surface to remove from, the view corner in this case */
                    auto cr_v = cairo_create(c->surf[active]);
                    cairo_set_operator(cr_v, CAIRO_OPERATOR_CLEAR);

                    if (do_round) {
                        /** Radius's center relative to the view's corner */
                        wf::point_t rc_rel_v;
                        rc_rel_v.x = -v_rel_a.x + p[j].x;
                        rc_rel_v.y = v_rel_a.y + c->g_rel.height - r + p[j].y;

                        cairo_move_to(cr_v, rc_rel_v);
                        cairo_arc(cr_v, rc_rel_v, r, angle, angle + M_PI / 2);
                        cairo_fill(cr_v);
                    } else {
                        /** Rectangle's origin, relative to the view's corner */
                        wf::point_t reo_rel_v;
                        reo_rel_v.x = -v_rel_a.x;
                        reo_rel_v.y = v_rel_a.y + c->g_rel.height - r;

                        cairo_rectangle(cr_v, reo_rel_v, r, r);
                        cairo_fill(cr_v);
                    }

                    /** Clear the view's corner with the accent rectangles */
                    for (auto cut : cuts) {
                        if (cut.width <= 0 || cut.height <= 0) { continue; }

                        /** Bottom of the rectangle relative to the view's corner */
                        wf::point_t reb_rel_v;
                        reb_rel_v.y = (c->g_rel.y + c->g_rel.height) -
                                      (cut.y + cut.height);
                        reb_rel_v.x = (cut.x - c->g_rel.x);

                        cairo_set_operator(cr_v, CAIRO_OPERATOR_CLEAR);
                        cairo_rectangle(cr_v, reb_rel_v, cut.width, cut.height);
                        cairo_fill(cr_v);
                    }
                            
                    cairo_surface_upload_to_texture(c->surf[active], c->tex[active]);
                    cairo_destroy(cr_v);
                }
            }

            /**** Final drawing of accent corner, overlaying the drawn rectangle */
            cairo_set_source_rgba(cr_a, a_color);
            cairo_set_operator(cr_a, CAIRO_OPERATOR_SOURCE);

            if (do_round) {
                cairo_move_to(cr_a, p[j]);
                cairo_arc(cr_a, p[j], r, angle, angle + M_PI / 2);
            } else {
                cairo_rectangle(cr_a, 0, 0, r, r);
            }
            /****/
            cairo_fill(cr_a);
            cairo_destroy(cr_a);
        }
        auto& texture = accent_textures.back();
        cairo_surface_upload_to_texture(surfaces[0], texture.tr[INACTIVE]);
        cairo_surface_upload_to_texture(surfaces[1], texture.tl[INACTIVE]);
        cairo_surface_upload_to_texture(surfaces[2], texture.bl[INACTIVE]);
        cairo_surface_upload_to_texture(surfaces[3], texture.br[INACTIVE]);
        cairo_surface_upload_to_texture(surfaces[4], texture.tr[ACTIVE]);
        cairo_surface_upload_to_texture(surfaces[5], texture.tl[ACTIVE]);
        cairo_surface_upload_to_texture(surfaces[6], texture.bl[ACTIVE]);
        cairo_surface_upload_to_texture(surfaces[7], texture.br[ACTIVE]);
        texture.radius = r;

        for (auto surface : surfaces) { cairo_surface_destroy(surface); }
    }

    void render_background_area(const wf::framebuffer_t& fb, wf::geometry_t g,
                                wf::point_t rect, wf::geometry_t scissor,
                                std::string rounded, unsigned long i,
                                wf::firedecor::decoration_area_type_t type) {
        /** The view's origin */                            
        wf::point_t o = { rect.x, rect.y };
        bool active = view->activated;

        if (type == wf::firedecor::DECORATION_AREA_ACCENT) {
            /**** Render the corners of an accent */
            int r;

            /** Create the corners, it should happen once per accent */
            if (accent_textures.size() <= i) {
                accent_textures.resize(i + 1);
                r = std::min({ ceil((double)g.height / 2), ceil((double)g.width / 2),
                               (double)corner_radius});
                form_accent_corners(r, g, rounded);
            }

            r = accent_textures.at(i).radius;

            wf::geometry_t c_g[] = { 
                { g.x + g.width - r, g.y, r, r }, { g.x, g.y, r, r },
                { g.x, g.y + g.height - r, r, r },
                { g.x + g.width - r, g.y + g.height - r, r, r }
            };

            OpenGL::render_begin(fb);
            fb.logic_scissor(scissor);
            for (int j = 0; j < 4; j++) {
            OpenGL::render_texture(accent_textures.at(i).tr[active].tex, fb,
                                   c_g[j] + o, glm::vec4(1.0));
            }
            /****/

            /**** Render the internal rectangles of the accent */
            wf::geometry_t accent_rects[] = { 
                { g.x + r, g.y, g.width - 2 * r, g.height },
                { g.x, g.y + r, r, g.height - 2 * r },
                { g.x + g.width - r, g.y + r, r, g.height - 2 * r }
            };

            wf::color_t color = (active) ? theme.get_accent_colors().active :
                                theme.get_accent_colors().inactive;
            color = alpha_transform(color);
            for (auto a_r : accent_rects) {
                if (a_r.width <= 0 || a_r.height <= 0) { continue; }
                OpenGL::render_rectangle(a_r + o, color,
                                         fb.get_orthographic_projection());
            }
            /****/
            OpenGL::render_end();
        } else {
            /**** Render a single rectangle when the area is a background */
            wf::color_t color = (active) ? theme.get_border_colors().active :
                                theme.get_border_colors().inactive;

            color = alpha_transform(color);

            OpenGL::render_begin(fb);
            fb.logic_scissor(scissor);
            OpenGL::render_rectangle(g + o, color, fb.get_orthographic_projection());
            OpenGL::render_end();
            /****/
        }
    }

	void render_background(const wf::framebuffer_t& fb, wf::geometry_t rect,
	                       const wf::geometry_t& scissor) {
		edge_colors_t colors = {
			theme.get_border_colors(), theme.get_outline_colors()
		};

		colors.border.active = alpha_transform(colors.border.active);
		colors.border.inactive = alpha_transform(colors.border.inactive);

		int r = theme.get_corner_radius() * fb.scale;
		update_corners(colors, r, fb.scale);

		int outline_size = theme.get_outline_size();

		/** Borders */
		unsigned long i = 0;
		wf::point_t rect_o = { rect.x, rect.y };
		for (auto area : layout.get_background_areas()) {
    		render_background_area(fb, area->get_geometry(), rect_o, scissor,
    		                       area->get_corners(), i, area->get_type());
    		i++;
		}

		OpenGL::render_begin(fb);
		fb.logic_scissor(scissor);

		/* Outlines */
		auto color = (view->activated) ? colors.outline.active :
	                 colors.outline.inactive;
		for (auto g : std::vector<wf::geometry_t>{
    		{ rect.x + corners.tl.r, rect.y,
    		  rect.width - (corners.tl.r + corners.tr.r), outline_size },
    		{ rect.x + corners.tr.r, rect.x + rect.height - outline_size ,
    		  rect.width - (corners.bl.r + corners.br.r), outline_size },
    		{ rect.x, rect.y + corners.tl.r, outline_size,
    		  rect.height - (corners.tl.r + corners.bl.r) },
    		{ rect.x + rect.width- outline_size, rect.y + corners.tr.r,
    		  outline_size, rect.height - (corners.tr.r + corners.br.r) } }) {
			OpenGL::render_rectangle(g, color, fb.get_orthographic_projection());
		}
        bool a = view->activated;
        wf::point_t o = { rect.x, rect.y };
		/** Rendering all corners */
		for (auto *c : { &corners.tr, &corners.tl, &corners.bl, &corners.br }) {
    		if (c->r <= 0) { continue; }
    		OpenGL::render_texture(c->tex[a].tex, fb, c->g_rel + o, glm::vec4(1.0f));
		}
		OpenGL::render_end();
	}

    void render_scissor_box(const wf::framebuffer_t& fb, wf::point_t origin,
                            const wlr_box& scissor) {
	    /* Draw the background (corners and border) */
        wlr_box geometry{origin.x, origin.y, size.width, size.height};
        render_background(fb, geometry, scissor);

        auto renderables = layout.get_renderable_areas();
        for (auto item : renderables) {
            int32_t bits;
            if (item->get_edge() == wf::firedecor::EDGE_LEFT) {
                bits = OpenGL::TEXTURE_TRANSFORM_INVERT_Y; 
            } else if (item->get_edge() == wf::firedecor::EDGE_RIGHT) {
                bits = OpenGL::TEXTURE_TRANSFORM_INVERT_X;
            }
	        if (item->get_type() == wf::firedecor::DECORATION_AREA_TITLE) {
                render_title(fb, item->get_geometry() + origin, 
                			 item->get_edge(), scissor);
            } else if (item->get_type() == wf::firedecor::DECORATION_AREA_BUTTON) {
	            item->as_button().set_active(view->activated);
	            item->as_button().set_maximized(view->tiled_edges);
                item->as_button().render(fb, item->get_geometry() + origin, scissor);
            } else if (item->get_type() == wf::firedecor::DECORATION_AREA_ICON) {
	            render_icon(fb, item->get_geometry() + origin, scissor, bits);
            }
        }
    }
    
    virtual void simple_render(const wf::framebuffer_t& fb, int x, int y,
					           const wf::region_t& damage) override {
        wf::region_t frame = this->cached_region + (wf::point_t){x, y};
        frame &= damage;

    	update_layout(DONT_FORCE);

        int h = std::max({ corner_radius, border_size.top, border_size.bottom });
        corners.tr.g_rel = { size.width - corner_radius, 0, corner_radius, h };
        corners.bl.g_rel = { 0, size.height - h, corner_radius, h };
        corners.br.g_rel = { size.width - corner_radius,
                             size.height - h, corner_radius, h };

        for (const auto& box : frame) {
            render_scissor_box(fb, {x, y}, wlr_box_from_pixman_box(box));
        }
    }

    bool accepts_input(int32_t sx, int32_t sy) override {
        return pixman_region32_contains_point(cached_region.to_pixman(),
            sx, sy, NULL);
    }

    virtual void on_pointer_enter(int x, int y) override {
        layout.handle_motion(x, y);
    }

    virtual void on_pointer_leave() override {
        layout.handle_focus_lost();
    }

    virtual void on_pointer_motion(int x, int y) override {
        handle_action(layout.handle_motion(x, y));
    }

    virtual void on_pointer_button(uint32_t button, uint32_t state) override {
        if (button != BTN_LEFT) {
            return;
        }

        handle_action(layout.handle_press_event(state == WLR_BUTTON_PRESSED));
    }

	// TODO: implement a pinning button.
    void handle_action(wf::firedecor::decoration_layout_t::action_response_t action) {
        switch (action.action) {
          case wf::firedecor::DECORATION_ACTION_MOVE:
            return view->move_request();

          case wf::firedecor::DECORATION_ACTION_RESIZE:
            return view->resize_request(action.edges);

          case wf::firedecor::DECORATION_ACTION_CLOSE:
            return view->close();

          case wf::firedecor::DECORATION_ACTION_TOGGLE_MAXIMIZE:
            if (view->tiled_edges) {
                view->tile_request(0);
            } else {
                view->tile_request(wf::TILED_EDGES_ALL);
            }

            break;

          case wf::firedecor::DECORATION_ACTION_MINIMIZE:
            view->minimize_request(true);
            break;

          default:
            break;
        }
    }

    virtual void on_touch_down(int x, int y) override {
        layout.handle_motion(x, y);
        handle_action(layout.handle_press_event());
    }

    virtual void on_touch_motion(int x, int y) override {
        handle_action(layout.handle_motion(x, y));
    }

    virtual void on_touch_up() override {
        handle_action(layout.handle_press_event(false));
        layout.handle_focus_lost();
    }

    void unmap() {
        _mapped = false;
        wf::emit_map_state_change(this);
    }

    void resize(wf::dimensions_t dims) {
        view->damage();
        size = dims;
		layout.resize(size.width, size.height, title.dims);
        if (!view->fullscreen) {
            this->cached_region = layout.calculate_region();
        }

        view->damage();
    }

    void update_decoration_size() {
        if (view->fullscreen) {
            border_size = { 0, 0, 0, 0 };
            this->cached_region.clear();
        } else {
            border_size = layout.parse_border(theme.get_border_size());
            this->cached_region = layout.calculate_region();
        }
    }
};

class simple_decorator_t : public wf::decorator_frame_t_t {
    wayfire_view view;
    nonstd::observer_ptr<simple_decoration_surface> deco;

  public:
    simple_decorator_t(wayfire_view view, wf::firedecor::theme_options options) {
        this->view = view;

        auto sub = std::make_unique<simple_decoration_surface>(view, options);
        deco = {sub};
        view->add_subsurface(std::move(sub), true);
        view->damage();
        view->connect_signal("subsurface-removed", &on_subsurface_removed);
    }

    ~simple_decorator_t() {
        if (deco) {
            // subsurface_removed unmaps it
            view->remove_subsurface(deco);
        }
    }

    simple_decorator_t(const simple_decorator_t &) = delete;
    simple_decorator_t(simple_decorator_t &&) = delete;
    simple_decorator_t& operator =(const simple_decorator_t&) = delete;
    simple_decorator_t& operator =(simple_decorator_t&&) = delete;

    wf::signal_connection_t on_subsurface_removed = [&] (auto data) {
        auto ev = static_cast<wf::subsurface_removed_signal*>(data);
        if (ev->subsurface.get() == deco.get()) {
            deco->unmap();
            deco = nullptr;
        }
    };

    virtual wf::geometry_t expand_wm_geometry( wf::geometry_t contained_wm_geometry) override {
        contained_wm_geometry.x     -= deco->border_size.left;
        contained_wm_geometry.y     -= deco->border_size.top;
        contained_wm_geometry.width += deco->border_size.left +
        	deco->border_size.right;
        contained_wm_geometry.height += deco->border_size.top +
        	deco->border_size.bottom;

        return contained_wm_geometry;
    }

    // TODO: Minimum size must fit buttons, icon, a truncated title, and corners.
    virtual void calculate_resize_size( int& target_width, int& target_height) override {
        target_width  -= deco->border_size.left +
        	deco->border_size.right;
        target_height -= deco->border_size.top + deco->border_size.bottom;

        target_width  = std::max(target_width, 1);
        target_height = std::max(target_height, 1);
    }

    virtual void notify_view_activated(bool active) override {
	    (void)active;
        view->damage();
    }

    virtual void notify_view_resized(wf::geometry_t view_geometry) override {
        deco->resize(wf::dimensions(view_geometry));
    }

    virtual void notify_view_tiled() override {}

    virtual void notify_view_fullscreen() override {
        deco->update_decoration_size();

        if (!view->fullscreen) {
            notify_view_resized(view->get_wm_geometry());
        }
    }
};

void init_view(wayfire_view view, wf::firedecor::theme_options options) {
    auto firedecor = std::make_unique<simple_decorator_t>(view, options);
    view->set_decoration(std::move(firedecor));
}

void deinit_view(wayfire_view view) {
    view->set_decoration(nullptr);
}
