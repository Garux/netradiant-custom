#include "terrain_generator.h"

#include <algorithm>
#include <cstdio>
#include "debugging/debugging.h"
#include "iplugin.h"
#include "string/string.h"
#include "modulesystem/singletonmodule.h"
#include "typesystem.h"

#include <QWidget>
#include <QDialog>
#include <QEventLoop>
#include <QTimer>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include "gtkutil/spinbox.h"
#include "gtkutil/combobox.h"

#include "scenelib.h"

#include "terrain_math.h"
#include "noise.h"
#include "terrain_engine.h"
#include "brush_builder.h"

#include "ibrush.h"
#include "ientity.h"
#include "ieclass.h"
#include "iscenegraph.h"
#include "iundo.h"
#include "iselection.h"
#include "qerplugin.h"
#include "modulesystem/moduleregistry.h"

namespace terrain_generator
{

QWidget* main_window;

const char* init( void* hApp, void* pMainWidget ){
	main_window = static_cast<QWidget*>( pMainWidget );
	return "";
}

const char* getName(){
	return "TerrainGenerator";
}

const char* getCommandList(){
	return "About;Generate Terrain";
}

const char* getCommandTitleList(){
	return "";
}

// The entity carrying terrain metadata for a selected instance: the instance's
// node if it is an entity, otherwise its parent (a selected brush's func_group).
static Entity* instance_entity( scene::Instance& instance ){
	const scene::Path& p = instance.path();
	if ( Node_isEntity( p.top() ) )
		return Node_getEntity( p.top().get() );
	if ( p.size() > 1 && Node_isEntity( p.parent() ) )
		return Node_getEntity( p.parent().get() );
	return nullptr;
}

void dispatch( const char* command, float* vMin, float* vMax, bool bSingleBrush ){
	if ( string_equal( command, "About" ) ) {
		GlobalRadiant().m_pfnMessageBox( main_window,
			"Terrain Generator\n\n"
			"Procedural CSG generation tool for id Tech 3 engines.\n\n"
			"Developed by vallz and vld",
			"About Terrain Generator",
			EMessageBoxType::Info, 0 );
		return;
	}
	if ( string_equal( command, "Generate Terrain" ) ) {
		// Only one generator dialog at a time. Re-running the command while
		// it's open just raises the existing window instead of stacking new ones.
		static QDialog* s_active_dialog = nullptr;
		if ( s_active_dialog ) {
			s_active_dialog->raise();
			s_active_dialog->activateWindow();
			return;
		}

		// Query the live selection bounds from the selection system.
		// Called at dialog-open time, on mode switch, and on OK — so the
		// user can make a selection while the dialog is open and retry.
		struct SelBounds { double x0, x1, y0, y1, z0, z1; bool valid; };
		auto query_sel = [&]( double min_size = 64.0 ) -> SelBounds {
			const AABB& b = GlobalSelectionSystem().getBoundsSelected();
			SelBounds s;
			s.x0 = b.origin[0] - b.extents[0];
			s.x1 = b.origin[0] + b.extents[0];
			s.y0 = b.origin[1] - b.extents[1];
			s.y1 = b.origin[1] + b.extents[1];
			s.z0 = b.origin[2] - b.extents[2];
			s.z1 = b.origin[2] + b.extents[2];
			s.valid = ( s.x1 - s.x0 >= min_size && s.y1 - s.y0 >= min_size );
			return s;
		};
		const SelBounds init_sel = query_sel();

		// --- Build dialog ---
		QDialog dialog( main_window, Qt::Dialog | Qt::WindowCloseButtonHint );
		dialog.setWindowTitle( "Terrain Generator" );
		s_active_dialog = &dialog;

		dialog.setMinimumWidth( 420 );
		auto *form = new QFormLayout( &dialog );

		// Target mode
		auto *target_combo = new ComboBox;
		target_combo->addItem( "Use Selection", 0 );
		target_combo->addItem( "Manual Size",   1 );
		target_combo->setCurrentIndex( init_sel.valid ? 0 : 1 );
		form->addRow( "Target:", target_combo );

		// Selection size — live, read-only labels showing the current selection.
		auto *sel_w_label = new QLabel;
		auto *sel_l_label = new QLabel;
		auto *sel_h_label = new QLabel;

		auto refresh_sel_labels = [&](){
			const SelBounds s = query_sel();
			sel_w_label->setText( s.valid ? QString::number( (int)( s.x1 - s.x0 ) )               : "—" );
			sel_l_label->setText( s.valid ? QString::number( (int)( s.y1 - s.y0 ) )               : "—" );
			sel_h_label->setText( s.valid ? QString::number( (int)std::max( s.z1 - s.z0, 64.0 ) ) : "—" );
		};
		refresh_sel_labels();

		form->addRow( "Width (X):",  sel_w_label );
		form->addRow( "Length (Y):", sel_l_label );
		form->addRow( "Height (Z):", sel_h_label );

		// Manual size inputs (shown in Manual Size mode)
		auto *manual_w_spin = new SpinBox( 64, 131072, 1024, 0, 64 );
		auto *manual_l_spin = new SpinBox( 64, 131072, 1024, 0, 64 );
		auto *manual_h_spin = new SpinBox( 64, 131072,   64, 0, 64 );
		form->addRow( "Width (X):",  manual_w_spin );
		form->addRow( "Length (Y):", manual_l_spin );
		form->addRow( "Height (Z):", manual_h_spin );

		// Sub-square size: preset combo + Advanced checkbox in one row
		auto *sq_widget  = new QWidget;
		auto *sq_hbox    = new QHBoxLayout( sq_widget );
		sq_hbox->setContentsMargins( 0, 0, 0, 0 );
		auto *sq_combo   = new ComboBox;
		for ( int v : { 8, 16, 32, 64, 128, 256, 512, 1024 } )
			sq_combo->addItem( QString::number( v ), v );
		sq_combo->setCurrentIndex( 3 ); // 64
		auto *sq_advanced = new QCheckBox( "Advanced" );
		sq_hbox->addWidget( sq_combo );
		sq_hbox->addWidget( sq_advanced );
		form->addRow( "Sub-square Size:", sq_widget );

		// Advanced step X/Y (hidden until Advanced is checked)
		auto *step_x_spin = new SpinBox( 8, 512, 64, 0, 8 );
		auto *step_y_spin = new SpinBox( 8, 512, 64, 0, 8 );
		form->addRow( "Step X:", step_x_spin );
		form->addRow( "Step Y:", step_y_spin );

		// Base shape
		auto *shape_combo = new ComboBox;
		shape_combo->addItem( "Flat (None)",      (int)ShapeType::Flat );
		shape_combo->addItem( "Hill",             (int)ShapeType::Hill );
		shape_combo->addItem( "Crater",           (int)ShapeType::Crater );
		shape_combo->addItem( "Ridge",            (int)ShapeType::Ridge );
		shape_combo->addItem( "Slope",            (int)ShapeType::Slope );
		shape_combo->addItem( "Volcano",          (int)ShapeType::Volcano );
		shape_combo->addItem( "Valley",           (int)ShapeType::Valley );
		shape_combo->addItem( "Tunnel",           (int)ShapeType::Tunnel );
		shape_combo->addItem( "Slope Tunnel",     (int)ShapeType::SlopeTunnel );
		shape_combo->setCurrentIndex( 0 ); // Flat
		form->addRow( "Base Shape:", shape_combo );

		// Shape height — editable spinbox (manual mode or non-slope shapes).
		// For Slope / Slope Tunnel in Use Selection mode, replaced by a
		// read-only label derived from the selection brush's Z extent.
		auto *shape_height_spin = new DoubleSpinBox( 0, 4096, 256, 2, 8 );
		form->addRow( "Peak Height:", shape_height_spin );

		// Tunnel height — only visible for Slope Tunnel
		auto *tunnel_height_spin = new DoubleSpinBox( 0, 4096, 256, 2, 8 );
		form->addRow( "Tunnel Height:", tunnel_height_spin );

		// Terrace step — hidden for Flat
		auto *terrace_spin = new DoubleSpinBox( 0, 512, 0, 2, 8 );
		form->addRow( "Terrace Step:", terrace_spin );

		// Noise type
		auto *noise_combo = new ComboBox;
		noise_combo->addItem( "Perlin Noise",     (int)NoiseType::Perlin );
		noise_combo->addItem( "Simplex Noise",    (int)NoiseType::Simplex );
		noise_combo->addItem( "Regular (Random)", (int)NoiseType::Random );
		form->addRow( "Noise Type:", noise_combo );

		// Variance / Frequency
		auto *variance_spin  = new DoubleSpinBox( 0, 1024, 32, 2, 1 );
		form->addRow( "Variance:", variance_spin );
		auto *frequency_spin = new DoubleSpinBox( 0.0001, 1.0, 0.005, 4, 0.001 );
		form->addRow( "Frequency:", frequency_spin );

		// Texture — line edit + Pick button to grab from texture browser
		auto *tex_widget = new QWidget;
		auto *tex_hbox   = new QHBoxLayout( tex_widget );
		tex_hbox->setContentsMargins( 0, 0, 0, 0 );
		auto *texture_edit = new QLineEdit( GlobalRadiant().TextureBrowser_getSelectedShader() );
		auto *tex_pick     = new QPushButton( "Pick" );
		tex_pick->setFixedWidth( 48 );
		tex_hbox->addWidget( texture_edit );
		tex_hbox->addWidget( tex_pick );

		// Poll the selection bounds every 250ms so the W/L/H labels stay
		// current if the user changes their selection while the dialog is open.
		auto *sel_timer = new QTimer( &dialog );
		sel_timer->setInterval( 250 );
		QObject::connect( sel_timer, &QTimer::timeout, [&](){
			if ( target_combo->currentIndex() == 0 )
				refresh_sel_labels();
		} );
		sel_timer->start();

		// Poll the texture browser every 100ms so picking a shader in it
		// updates the texture field automatically.
		QString last_polled_shader = texture_edit->text();

		auto *pick_timer = new QTimer( &dialog );
		pick_timer->setInterval( 100 );
		QObject::connect( pick_timer, &QTimer::timeout, [&](){
			const QString current = GlobalRadiant().TextureBrowser_getSelectedShader();
			if ( current != last_polled_shader ) {
				last_polled_shader = current;
				texture_edit->setText( current );
			}
		} );
		pick_timer->start();

		// Surface the texture browser and leave it open so the user can keep
		// picking. Closing it automatically proved fragile and is poor UX.
		// A tooltip covers the embedded-layout case, where there is no separate
		// browser window to raise (it's always visible in the main window).
		tex_pick->setToolTip( "Open the texture browser to pick a shader.\n"
		                      "If the browser is docked in the main window layout,\n"
		                      "select a texture there and it fills in here automatically." );
		QObject::connect( tex_pick, &QPushButton::clicked, [&](){
			GlobalRadiant().TextureBrowser_show();
		} );
		form->addRow( "Texture:", tex_widget );

		// Generate (generate without closing) + Close
		// Buttons: Generate (left) — Close (right), explicit layout so the
		// order is platform-independent.
		auto *btn_widget   = new QWidget;
		auto *btn_layout   = new QHBoxLayout( btn_widget );
		btn_layout->setContentsMargins( 0, 12, 0, 0 ); // top spacing from fields
		auto *generate_btn = new QPushButton( "Generate" );
		auto *close_btn    = new QPushButton( "Close" );
		btn_layout->addStretch();
		btn_layout->addWidget( generate_btn );
		btn_layout->addSpacing( 8 );
		btn_layout->addWidget( close_btn );
		btn_layout->addStretch();
		form->addRow( btn_widget );

		// Helper: show/hide a form row (widget + its label)
		auto set_row_visible = [&]( QWidget *w, bool visible ){
			w->setVisible( visible );
			if ( auto *lbl = form->labelForField( w ) )
				lbl->setVisible( visible );
		};

		// --- Per-entity persisted footprint size ------------------------------
		// Generated terrain records the footprint SIZE it was built from as a key
		// on its func_group(s). On regeneration we reuse that stored size (so the
		// terrain stops growing) but take the position from the live selection (so
		// it follows wherever the terrain was moved). The size lives on each piece
		// of terrain, so a second brush never picks up the first one's size.
		const char* const TERRAINGEN_KEY = "_terraingen_size";

		// Fills w/l/h and returns true if any selected node (or its parent entity)
		// carries a stored footprint size.
		auto read_stored_size = [&]( double& w, double& l, double& h ) -> bool {
			struct Reader : public SelectionSystem::Visitor {
				const char* key;
				double *w, *l, *h;
				mutable bool found = false;
				Reader( const char* k, double* a, double* b, double* c ) : key( k ), w( a ), l( b ), h( c ){}
				void visit( scene::Instance& instance ) const override {
					if ( found )
						return;
					Entity* e = instance_entity( instance );
					if ( e == nullptr )
						return;
					const char* v = e->getKeyValue( key );
					if ( v != nullptr && v[0] != '\0'
					     && std::sscanf( v, "%lf %lf %lf", w, l, h ) == 3
					     && *w >= 1.0 && *l >= 1.0 )
						found = true;
				}
			} reader( TERRAINGEN_KEY, &w, &l, &h );
			GlobalSelectionSystem().foreachSelected( reader );
			return reader.found;
		};

		// Writes the footprint size onto the entity of every selected node (the
		// freshly generated func_group(s), whose brushes the build step selects).
		auto write_stored_size = [&]( const SelBounds& s ){
			char buf[128];
			std::snprintf( buf, sizeof( buf ), "%g %g %g",
			               s.x1 - s.x0, s.y1 - s.y0, s.z1 - s.z0 );
			struct Writer : public SelectionSystem::Visitor {
				const char* key;
				const char* value;
				Writer( const char* k, const char* v ) : key( k ), value( v ){}
				void visit( scene::Instance& instance ) const override {
					if ( Entity* e = instance_entity( instance ) )
						e->setKeyValue( key, value );
				}
			} writer( TERRAINGEN_KEY, buf );
			GlobalSelectionSystem().foreachSelected( writer );
		};
		// ----------------------------------------------------------------------

		// Shared validation + generation — returns true on success.
		// Called by both Generate and OK.
		auto do_generate = [&]() -> bool {
			if ( target_combo->currentIndex() == 0 ) {
				if ( GlobalSelectionSystem().countSelected() == 0 ) {
					GlobalRadiant().m_pfnMessageBox( main_window,
						"No brush is selected.\n\n"
						"Please select a brush in the viewport first,\n"
						"or switch the Target to \"Manual Size\".",
						"Terrain Generator - No Selection",
						EMessageBoxType::Error, 0 );
					return false;
				}
				if ( !query_sel().valid ) {
					GlobalRadiant().m_pfnMessageBox( main_window,
						"The selected brush is too small.\n\n"
						"The terrain area must be at least 64 x 64 units.\n"
						"Please select a larger brush,\n"
						"or switch the Target to \"Manual Size\".",
						"Terrain Generator - Brush Too Small",
						EMessageBoxType::Error, 0 );
					return false;
				}
			}
			if ( texture_edit->text().trimmed().isEmpty() ) {
				GlobalRadiant().m_pfnMessageBox( main_window,
					"No texture specified.\n\n"
					"Please enter a texture path or use the Pick button\n"
					"to select one from the texture browser.",
					"Terrain Generator - No Texture",
					EMessageBoxType::Error, 0 );
				return false;
			}

			// --- Read parameters ---
			const bool use_manual  = ( target_combo->currentIndex() == 1 );
			const bool advanced    = sq_advanced->isChecked();
			const double step_x    = advanced ? step_x_spin->value() : sq_combo->currentData().toInt();
			const double step_y    = advanced ? step_y_spin->value() : sq_combo->currentData().toInt();
			const ShapeType shape  = (ShapeType)shape_combo->currentData().toInt();
			const NoiseType noise  = (NoiseType)noise_combo->currentData().toInt();
			const double tun_height = tunnel_height_spin->value();
			const double variance     = variance_spin->value();
			const double frequency    = frequency_spin->value();
			const double terrace      = terrace_spin->value();
			const std::string texture_str = texture_edit->text().toStdString();
			const char* texture       = texture_str.c_str();

			// --- Determine bounds (must happen before deleting the selection brush) ---
			BrushData target;

			// Footprint to record on the generated entities, so a later
			// regeneration reuses it instead of growing.
			SelBounds used_bounds{};
			bool      used_bounds_valid = false;

			if ( use_manual ) {
				target = make_manual_brush_data( manual_w_spin->value(), manual_l_spin->value(), manual_h_spin->value() );
			}
			else {
				// Reuse the stored footprint size (so regeneration doesn't grow),
				// but take the position from the live selection (so it follows the
				// terrain if it was moved). Peaks and walls extend symmetrically,
				// so the live center equals the footprint center; the floor sits at
				// the live minimum Z. A fresh brush has no stored size and uses its
				// own bounds directly.
				double sw, sl, sh;
				SelBounds s{};
				if ( read_stored_size( sw, sl, sh ) ) {
					const SelBounds live = query_sel();
					if ( live.valid ) {
						const double cx = ( live.x0 + live.x1 ) * 0.5;
						const double cy = ( live.y0 + live.y1 ) * 0.5;
						s.x0 = cx - sw * 0.5; s.x1 = cx + sw * 0.5;
						s.y0 = cy - sl * 0.5; s.y1 = cy + sl * 0.5;
						s.z0 = live.z0;       s.z1 = live.z0 + sh;
						s.valid = true;
					}
				}
				else {
					s = query_sel( step_x < step_y ? step_x : step_y );
				}
				if ( s.valid ) {
					target.min_x = s.x0;
					target.max_x = s.x1;
					target.min_y = s.y0;
					target.max_y = s.y1;
					target.min_z = s.z0;
					// Base level the terrain builds up from. Slopes use the slope
					// height so the ramp's top rises with it while its low edge
					// stays at the floor (z0 + 64); otherwise a steep slope sinks
					// the low edge below the floor, clips away geometry and makes
					// regeneration drift sideways. Other shapes use the selection
					// height.
					const bool is_slope = ( shape == ShapeType::Slope || shape == ShapeType::SlopeTunnel );
					const double top_h  = is_slope ? std::max( shape_height_spin->value(), 64.0 )
					                               : std::max( s.z1 - s.z0, 64.0 );
					target.max_z    = s.z0 + top_h;
					target.width_x  = target.max_x - target.min_x;
					target.length_y = target.max_y - target.min_y;
					target.height_z = target.max_z - target.min_z;

					// Remember the footprint to record on the generated entities
					// once the build step completes.
					used_bounds       = s;
					used_bounds_valid = true;
				}
				else {
					target = make_manual_brush_data( manual_w_spin->value(), manual_l_spin->value(), manual_h_spin->value() );
				}
			}

			// For Slope / Slope Tunnel in Use Selection mode, derive the slope
			// height from the brush's Z extent so the terrain descends from the
			// top of the brush down to a minimum height of 64 units.
			// A negative value flips the engine's formula (base_z = height * nx)
			// so it slopes downward instead of upward.
			// For Slope/SlopeTunnel in Use Selection mode the spinbox holds the
			// drop amount (auto-filled from brush Z − 64). Negate it so the
			// engine formula (base_z = shape_height * nx) slopes downward.
			const bool   slope_from_sel = !use_manual
			                           && ( shape == ShapeType::Slope || shape == ShapeType::SlopeTunnel );
			// Spinbox shows the full brush Z height. For the downward slope the
			// engine needs the drop amount (full_z − 64), negated so the
			// formula base_z = shape_height*nx descends to min_z + 64.
			const double shape_height   = slope_from_sel
			                           ? -( shape_height_spin->value() - 64.0 )
			                           : shape_height_spin->value();

			// One undo step covering both removing the source brush and creating
			// the generated geometry, so a single Undo restores the original
			// brush and removes everything we added.
			UndoableCommand undo( "terrainGenerator.generate" );

			// --- Delete the selection brush now that its bounds are captured ---
			if ( !use_manual ) {
				while ( GlobalSelectionSystem().countSelected() > 0 ) {
					scene::Path path( GlobalSelectionSystem().ultimateSelected().path() );
					scene::Path parent_path( path );
					parent_path.pop();

					Path_deleteTop( path );

					// Remove a group entity left empty by the deletion, mirroring
					// Radiant's own delete behavior (never the worldspawn or root).
					if ( parent_path.size() > 1
					     && Node_isEntity( parent_path.top() )
					     && !parent_path.top().get().isRoot() ) {
						Entity* entity = Node_getEntity( parent_path.top() );
						scene::Traversable* children = Node_getTraversable( parent_path.top() );
						const bool worldspawn = entity != 0 && string_equal( entity->getClassName(), "worldspawn" );
						if ( !worldspawn && children != 0 && children->empty() )
							Path_deleteTop( parent_path );
					}
				}
				SceneChangeNotify();
			}

			adjust_bounds_to_fit_grid( target, step_x, step_y );

			// Clear any leftover selection so that only the geometry we are
			// about to generate ends up selected (the build step selects it).
			GlobalSelectionSystem().setSelectedAll( false );

			// --- Generate ---
			const bool is_tunnel = ( shape == ShapeType::Tunnel || shape == ShapeType::SlopeTunnel );

			globalOutputStream() << "TerrainGenerator: generating "
			                     << ( is_tunnel ? "tunnel" : "terrain" )
			                     << " - bounds ("
			                     << target.width_x << " x " << target.length_y << " x " << target.height_z
			                     << "), step (" << step_x << " x " << step_y << ")"
			                     << ", texture: " << texture << "\n";

			if ( is_tunnel ) {
				const double cave_height    = ( shape == ShapeType::SlopeTunnel ) ? tun_height   : shape_height;
				const double slope_height   = ( shape == ShapeType::SlopeTunnel ) ? shape_height : 0;
				const double tunnel_terrace = ( shape == ShapeType::SlopeTunnel ) ? terrace      : 0.0;
				auto maps = generate_tunnel_height_maps( target, step_x, step_y, cave_height, slope_height, variance, frequency, noise, tunnel_terrace );
				build_tunnel_brushes( target, step_x, step_y, maps, texture, cave_height, slope_height );
			}
			else {
				bool split_diagonally = ( variance > 0 || shape != ShapeType::Flat );
				auto height_map = generate_height_map( target, step_x, step_y, shape, shape_height, variance, frequency, noise, terrace );
				build_terrain_brushes( target, step_x, step_y, height_map, texture, split_diagonally );
			}

			globalOutputStream() << "TerrainGenerator: generation complete\n";

			// Record the footprint size on the generated (now selected) entities so
			// a future regeneration reuses it instead of the grown geometry's size.
			if ( used_bounds_valid )
				write_stored_size( used_bounds );

			return true;
		};

		QObject::connect( generate_btn, &QPushButton::clicked, [&](){ do_generate(); } );
		QObject::connect( close_btn,    &QPushButton::clicked, &dialog, &QDialog::reject );

		// Shape height label per shape type (index matches ShapeType enum value)
		static const char* shape_height_label[] = {
			nullptr,           // Flat — row hidden
			"Peak Height:",    // Hill
			"Crater Depth:",   // Crater
			"Ridge Height:",   // Ridge
			"Slope Height:",   // Slope
			"Volcano Height:", // Volcano
			"Valley Depth:",   // Valley
			"Tunnel Height:",  // Tunnel
			"Slope Height:",   // SlopeTunnel
		};

		// Returns true when slope height should be derived from the selection
		// (Use Selection mode + Slope or Slope Tunnel shape).
		auto slope_derived = [&]() -> bool {
			const ShapeType st = (ShapeType)shape_combo->currentData().toInt();
			return target_combo->currentIndex() == 0
			    && ( st == ShapeType::Slope || st == ShapeType::SlopeTunnel );
		};

		// Target mode toggle: read-only selection labels vs editable spinboxes.
		// Refresh labels each time the user switches to "Use Selection" so they
		// reflect whatever is selected at that moment.
		auto update_target_mode = [&]( int idx ){
			const bool use_sel = ( idx == 0 );
			if ( use_sel ) {
				refresh_sel_labels();
				if ( slope_derived() ) {
					// Derive the slope height from the stored footprint height when
					// the selection has one, so it stays consistent with the size
					// used for generation. Using the live (grown) selection here
					// over-steepens the slope and pushes the floor below the base.
					double sw, sl, sh;
					if ( read_stored_size( sw, sl, sh ) ) {
						shape_height_spin->setValue( sh );
					}
					else {
						const SelBounds s = query_sel();
						if ( s.valid )
							shape_height_spin->setValue( s.z1 - s.z0 );
					}
				}
			}
			set_row_visible( sel_w_label,   use_sel );
			set_row_visible( sel_l_label,   use_sel );
			set_row_visible( sel_h_label,   use_sel );
			set_row_visible( manual_w_spin, !use_sel );
			set_row_visible( manual_l_spin, !use_sel );
			set_row_visible( manual_h_spin, !use_sel );
		};

		// Advanced sub-square toggle
		auto update_advanced = [&]( bool advanced ){
			sq_combo->setVisible( !advanced );
			set_row_visible( step_x_spin, advanced );
			set_row_visible( step_y_spin, advanced );
		};

		// Shape type toggle: labels + visibility
		auto update_shape = [&]( int idx ){
			const ShapeType st         = (ShapeType)shape_combo->itemData( idx ).toInt();
			const bool is_flat         = ( st == ShapeType::Flat );
			const bool is_slope_tunnel = ( st == ShapeType::SlopeTunnel );

			set_row_visible( shape_height_spin, !is_flat );
			if ( !is_flat ) {
				if ( auto *lbl = qobject_cast<QLabel*>( form->labelForField( shape_height_spin ) ) )
					lbl->setText( shape_height_label[idx] );
				// Auto-fill slope height from selection when applicable
				if ( slope_derived() ) {
					// Derive the slope height from the stored footprint height when
					// the selection has one, so it stays consistent with the size
					// used for generation. Using the live (grown) selection here
					// over-steepens the slope and pushes the floor below the base.
					double sw, sl, sh;
					if ( read_stored_size( sw, sl, sh ) ) {
						shape_height_spin->setValue( sh );
					}
					else {
						const SelBounds s = query_sel();
						if ( s.valid )
							shape_height_spin->setValue( s.z1 - s.z0 );
					}
				}
			}
			set_row_visible( tunnel_height_spin, is_slope_tunnel );
			// Terrace not applicable to flat tunnels (no slope to step),
			// but valid for slope tunnels where the floor descends along Y
			set_row_visible( terrace_spin, !is_flat && st != ShapeType::Tunnel );
		};

		// Wire signals
		QObject::connect( target_combo, QOverload<int>::of( &QComboBox::currentIndexChanged ), update_target_mode );
		QObject::connect( sq_advanced,  &QCheckBox::toggled,                                   update_advanced );
		QObject::connect( shape_combo,  QOverload<int>::of( &QComboBox::currentIndexChanged ), update_shape );

		// Set initial visibility
		update_target_mode( target_combo->currentIndex() );
		update_advanced( false );
		update_shape( shape_combo->currentIndex() );

		// show() instead of exec() so the dialog is non-modal — the texture
		// browser panel (and all other Radiant windows) remain interactive
		dialog.show();
		dialog.raise();
		{
			QEventLoop loop;
			QObject::connect( &dialog, &QDialog::finished, &loop, &QEventLoop::quit );
			loop.exec();
		}
		s_active_dialog = nullptr;
	}
}

} // namespace terrain_generator

class TerrainGeneratorDependencies :
	public GlobalRadiantModuleRef,
	public GlobalUndoModuleRef,
	public GlobalSceneGraphModuleRef,
	public GlobalSelectionModuleRef,
	public GlobalEntityModuleRef,
	public GlobalEntityClassManagerModuleRef,
	public GlobalBrushModuleRef
{
public:
	TerrainGeneratorDependencies() :
		GlobalEntityModuleRef( GlobalRadiant().getRequiredGameDescriptionKeyValue( "entities" ) ),
		GlobalEntityClassManagerModuleRef( GlobalRadiant().getRequiredGameDescriptionKeyValue( "entityclass" ) ),
		GlobalBrushModuleRef( GlobalRadiant().getRequiredGameDescriptionKeyValue( "brushtypes" ) ){
	}
};

class TerrainGeneratorModule : public TypeSystemRef
{
	_QERPluginTable m_plugin;
public:
	typedef _QERPluginTable Type;
	STRING_CONSTANT( Name, "TerrainGenerator" );

	TerrainGeneratorModule(){
		m_plugin.m_pfnQERPlug_Init                = &terrain_generator::init;
		m_plugin.m_pfnQERPlug_GetName             = &terrain_generator::getName;
		m_plugin.m_pfnQERPlug_GetCommandList      = &terrain_generator::getCommandList;
		m_plugin.m_pfnQERPlug_GetCommandTitleList = &terrain_generator::getCommandTitleList;
		m_plugin.m_pfnQERPlug_Dispatch            = &terrain_generator::dispatch;
	}

	_QERPluginTable* getTable(){
		return &m_plugin;
	}
};

typedef SingletonModule<TerrainGeneratorModule, TerrainGeneratorDependencies> SingletonTerrainGeneratorModule;
SingletonTerrainGeneratorModule g_TerrainGeneratorModule;

extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer& server ){
	initialiseModule( server );
	g_TerrainGeneratorModule.selfRegister();
}
