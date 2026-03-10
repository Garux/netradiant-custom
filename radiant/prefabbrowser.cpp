/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "prefabbrowser.h"

#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <filesystem>
#include <system_error>
#include <string>
#include <vector>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <QBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QLabel>
#include <QListView>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QLinearGradient>
#include <QFont>
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QApplication>
#include <QTimer>
#include <QFileInfo>
#include <QDir>
#include <QThreadPool>
#include <QRunnable>
#include <QThread>
#include <QTreeWidget>
#include <QHeaderView>
#include <QSplitter>
#include <QComboBox>

#include "stream/stringstream.h"
#include "os/file.h"
#include "os/path.h"
#include "math/vector.h"
#include "math/plane.h"
#include "preferencesystem.h"
#include "stringio.h"
#include "map.h"
#include "entity.h"
#include "select.h"
#include "gtkutil/image.h"

namespace
{
constexpr const char* c_prefabMimeType = "application/x-netradiant-prefab";

class PrefabListWidget final : public QListWidget
{
	QListWidgetItem* m_dragItem = nullptr;
	QPoint m_dragStartPos;
public:
	using QListWidget::QListWidget;

	void beginDragForItem( QListWidgetItem* item ){
		if( item == nullptr ){
			return;
		}

		const QString model = item->data( Qt::ItemDataRole::UserRole ).toString();
		if( model.isEmpty() ){
			return;
		}

		auto* mimeData = new QMimeData;
		const QByteArray utf8 = model.toUtf8();
		mimeData->setData( c_prefabMimeType, utf8 );
		mimeData->setText( model );

		auto* drag = new QDrag( this );
		drag->setMimeData( mimeData );
		drag->setHotSpot( QPoint( 16, 16 ) );
		drag->exec( Qt::DropAction::CopyAction );
	}

	void mousePressEvent( QMouseEvent* event ) override {
		m_dragItem = itemAt( event->pos() );
		m_dragStartPos = event->pos();
		if( m_dragItem != nullptr ){
			setCurrentItem( m_dragItem );
		}
		QListWidget::mousePressEvent( event );
	}
	void mouseMoveEvent( QMouseEvent* event ) override {
		if( ( event->buttons() & Qt::MouseButton::LeftButton ) && m_dragItem != nullptr ){
			const int distance = ( event->pos() - m_dragStartPos ).manhattanLength();
			if( distance >= QApplication::startDragDistance() ){
				beginDragForItem( m_dragItem );
				m_dragItem = nullptr;
				return;
			}
		}
		QListWidget::mouseMoveEvent( event );
	}
	void mouseReleaseEvent( QMouseEvent* event ) override {
		m_dragItem = nullptr;
		QListWidget::mouseReleaseEvent( event );
	}

	void startDrag( Qt::DropActions supportedActions ) override {
		Q_UNUSED( supportedActions );
		QListWidgetItem* item = m_dragItem;
		if( item == nullptr ){
			const QList<QListWidgetItem*> items = selectedItems();
			item = items.empty() ? nullptr : items.front();
		}
		beginDragForItem( item );
	}

	QStringList mimeTypes() const override {
		QStringList types = QListWidget::mimeTypes();
		types.push_back( c_prefabMimeType );
		return types;
	}

	QMimeData* mimeData( const QList<QListWidgetItem*> items ) const override {
		QMimeData* data = QListWidget::mimeData( items );
		if( items.empty() || data == nullptr ){
			return data;
		}

		const QString model = items.front()->data( Qt::ItemDataRole::UserRole ).toString();
		if( !model.isEmpty() ){
			const QByteArray utf8 = model.toUtf8();
			data->setData( c_prefabMimeType, utf8 );
			data->setText( model );
		}
		return data;
	}
};

struct PrefabEntry
{
	std::string modelPath;
	std::string absolutePath;
	std::string previewPath;
};

struct ProjectedPoint
{
	QPointF p;
	float depth;
};

class PrefabBrowser
{
public:
	QTreeWidget* m_tree = nullptr;
	QLineEdit* m_filter = nullptr;
	QListWidget* m_list = nullptr;
	QLabel* m_info = nullptr;
	std::vector<PrefabEntry> m_prefabs;
	std::string m_currentFolder;
	std::set<std::string> m_previewInFlight;
	int m_previewSize = 96;
};

PrefabBrowser g_prefabBrowser;
QIcon g_prefabFallbackIcon;

QIcon PrefabBrowser_placeholderIcon( int size ){
	const int clamped = std::max( 64, std::min( 224, size ) );
	if( !g_prefabFallbackIcon.isNull() ){
		const QPixmap px = g_prefabFallbackIcon.pixmap( clamped, clamped );
		if( !px.isNull() ){
			return QIcon( px );
		}
	}

	QPixmap pixmap( clamped, clamped );
	pixmap.fill( QColor( 28, 31, 36 ) );
	return QIcon( pixmap );
}

struct PathStringLess
{
	bool operator()( const std::string& a, const std::string& b ) const {
		return path_less( a.c_str(), b.c_str() );
	}
};

inline bool has_map_extension( const std::filesystem::path& path ){
	const std::string ext = path.extension().string();
	return string_equal_nocase( ext.c_str(), ".map" );
}

inline bool should_show_prefab( const std::string& prefabPath, const QString& filter ){
	if( filter.isEmpty() ){
		return true;
	}
	return QString::fromUtf8( prefabPath.c_str() ).contains( filter, Qt::CaseInsensitive );
}

std::string clean_path( const std::string& path ){
	return StringStream( PathCleaned( path.c_str() ) ).c_str();
}

std::string maps_relative_to_prefab_model( const std::string& absolutePath ){
	const char* relative = path_make_relative( absolutePath.c_str(), getMapsPath() );
	if( relative != absolutePath.c_str() ){
		return StringStream( "maps/", PathCleaned( relative ) ).c_str();
	}
	return StringStream( PathCleaned( absolutePath.c_str() ) ).c_str();
}

Vector3 prefab_insert_origin(){
	const select_workzone_t& workzone = Select_getWorkZone();
	return Vector3(
		( workzone.d_work_min[0] + workzone.d_work_max[0] ) * 0.5f,
		( workzone.d_work_min[1] + workzone.d_work_max[1] ) * 0.5f,
		( workzone.d_work_min[2] + workzone.d_work_max[2] ) * 0.5f
	);
}

bool generate_preview_png( const std::string& mapPath, const std::string& previewPath ){
	if( file_exists( previewPath.c_str() ) ){
		return true;
	}

	auto read_all_file_text = []( const char* path, std::string& out ) -> bool {
		std::FILE* f = std::fopen( path, "rb" );
		if( f == nullptr ){
			return false;
		}
		if( std::fseek( f, 0, SEEK_END ) != 0 ){
			std::fclose( f );
			return false;
		}
		const long size = std::ftell( f );
		if( size < 0 ){
			std::fclose( f );
			return false;
		}
		if( std::fseek( f, 0, SEEK_SET ) != 0 ){
			std::fclose( f );
			return false;
		}

		out.clear();
		out.resize( static_cast<std::size_t>( size ) );
		if( size > 0 ){
			const std::size_t read = std::fread( &out[0], 1, static_cast<std::size_t>( size ), f );
			if( read != static_cast<std::size_t>( size ) ){
				std::fclose( f );
				return false;
			}
		}
		std::fclose( f );
		return true;
	};
	auto parse_vec3_at = []( const char* p, Vector3& out, const char** outNext ) -> bool {
		const char* cur = p;
		while( *cur != '\0' && std::isspace( static_cast<unsigned char>( *cur ) ) ){
			++cur;
		}
		if( *cur != '(' ){
			return false;
		}
		++cur;

		char* end = nullptr;
		errno = 0;
		const double x = std::strtod( cur, &end );
		if( end == cur || errno != 0 ){
			return false;
		}
		cur = end;

		errno = 0;
		const double y = std::strtod( cur, &end );
		if( end == cur || errno != 0 ){
			return false;
		}
		cur = end;

		errno = 0;
		const double z = std::strtod( cur, &end );
		if( end == cur || errno != 0 ){
			return false;
		}
		cur = end;

		while( *cur != '\0' && std::isspace( static_cast<unsigned char>( *cur ) ) ){
			++cur;
		}
		if( *cur != ')' ){
			return false;
		}
		++cur;
		out = Vector3( static_cast<float>( x ), static_cast<float>( y ), static_cast<float>( z ) );
		*outNext = cur;
		return true;
	};
	auto project_iso = []( const Vector3& p ) -> ProjectedPoint {
		const float yaw = 0.78539816339f;
		const float pitch = 0.61086523819f;
		const float cy = std::cos( yaw );
		const float sy = std::sin( yaw );
		const float cp = std::cos( pitch );
		const float sp = std::sin( pitch );

		const float rx = cy * p[0] - sy * p[1];
		const float ry = sy * p[0] + cy * p[1];
		const float rz = p[2];
		const float ryp = cp * ry - sp * rz;
		const float depth = sp * ry + cp * rz;
		return { QPointF( rx, ryp ), depth };
	};
	auto hash_shader = []( const std::string& s ) -> unsigned {
		unsigned h = 2166136261u;
		for( char c : s ){
			h ^= static_cast<unsigned char>( c );
			h *= 16777619u;
		}
		return h;
	};
	auto shader_color = [&]( const std::string& shader ) -> QColor {
		const unsigned h = hash_shader( shader );
		const int r = 60 + static_cast<int>( h & 0x7F );
		const int g = 60 + static_cast<int>( ( h >> 8 ) & 0x7F );
		const int b = 60 + static_cast<int>( ( h >> 16 ) & 0x7F );
		return QColor( r, g, b, 120 );
	};

	struct FacePreviewTri
	{
		Vector3 a;
		Vector3 b;
		Vector3 c;
		QColor color;
	};
	struct BrushFace
	{
		Plane3 plane;
		QColor color;
	};

	auto intersect_three_planes = []( const Plane3& p0, const Plane3& p1, const Plane3& p2, Vector3& out ) -> bool {
		const Vector3 n0 = p0.normal();
		const Vector3 n1 = p1.normal();
		const Vector3 n2 = p2.normal();
		const Vector3 n1xn2 = vector3_cross( n1, n2 );
		const double denom = vector3_dot( n0, n1xn2 );
		if( std::fabs( denom ) < 1e-8 ){
			return false;
		}

		const Vector3 term0 = vector3_scaled( n1xn2, static_cast<float>( p0.dist() ) );
		const Vector3 term1 = vector3_scaled( vector3_cross( n2, n0 ), static_cast<float>( p1.dist() ) );
		const Vector3 term2 = vector3_scaled( vector3_cross( n0, n1 ), static_cast<float>( p2.dist() ) );
		out = vector3_scaled( vector3_added( vector3_added( term0, term1 ), term2 ), static_cast<float>( 1.0 / denom ) );
		return true;
	};
	auto push_unique_point = []( std::vector<Vector3>& points, const Vector3& point ){
		for( const Vector3& existing : points ){
			if( vector3_length_squared( vector3_subtracted( existing, point ) ) < 0.01 ){
				return;
			}
		}
		points.push_back( point );
	};
	auto append_brush_triangles = [&]( const std::vector<BrushFace>& brushFaces, std::vector<FacePreviewTri>& outTris, std::vector<Vector3>& outPoints ){
		if( brushFaces.size() < 4 ){
			return;
		}

		for( std::size_t i = 0; i < brushFaces.size(); ++i ){
			const BrushFace& face = brushFaces[i];
			std::vector<Vector3> polygon;
			polygon.reserve( 32 );

			for( std::size_t j = 0; j < brushFaces.size(); ++j ){
				if( j == i ){
					continue;
				}
				for( std::size_t k = j + 1; k < brushFaces.size(); ++k ){
					if( k == i ){
						continue;
					}

					Vector3 candidate;
					if( !intersect_three_planes( face.plane, brushFaces[j].plane, brushFaces[k].plane, candidate ) ){
						continue;
					}

					bool inside = true;
					for( const BrushFace& testFace : brushFaces ){
						if( plane3_distance_to_point( testFace.plane, candidate ) > 0.08 ){
							inside = false;
							break;
						}
					}
					if( !inside ){
						continue;
					}
					if( std::fabs( plane3_distance_to_point( face.plane, candidate ) ) > 0.08 ){
						continue;
					}

					push_unique_point( polygon, candidate );
				}
			}

			if( polygon.size() < 3 ){
				continue;
			}

			Vector3 centroid( 0, 0, 0 );
			for( const Vector3& p : polygon ){
				centroid += p;
			}
			centroid = vector3_scaled( centroid, 1.f / static_cast<float>( polygon.size() ) );

			Vector3 helper = std::fabs( face.plane.normal()[2] ) < 0.95 ? g_vector3_axis_z : g_vector3_axis_x;
			Vector3 axisU = vector3_cross( helper, face.plane.normal() );
			if( vector3_length_squared( axisU ) < 1e-8 ){
				helper = g_vector3_axis_y;
				axisU = vector3_cross( helper, face.plane.normal() );
			}
			axisU = vector3_normalised( axisU );
			const Vector3 axisV = vector3_cross( face.plane.normal(), axisU );

			std::sort( polygon.begin(), polygon.end(), [&]( const Vector3& a, const Vector3& b ){
				const Vector3 ra = vector3_subtracted( a, centroid );
				const Vector3 rb = vector3_subtracted( b, centroid );
				const double aa = std::atan2( vector3_dot( ra, axisV ), vector3_dot( ra, axisU ) );
				const double ab = std::atan2( vector3_dot( rb, axisV ), vector3_dot( rb, axisU ) );
				return aa < ab;
			} );

			for( std::size_t t = 1; t + 1 < polygon.size(); ++t ){
				outTris.push_back( FacePreviewTri{ polygon[0], polygon[t], polygon[t + 1], face.color } );
				outPoints.push_back( polygon[0] );
				outPoints.push_back( polygon[t] );
				outPoints.push_back( polygon[t + 1] );
			}
		}
	};

	std::string text;
	std::vector<FacePreviewTri> faces;
	std::vector<Vector3> points;
	if( read_all_file_text( mapPath.c_str(), text ) ){
		int depth = 0;
		std::vector<BrushFace> brushFaces;
		std::size_t lineStart = 0;
		while( lineStart < text.size() ){
			const std::size_t lineEnd = text.find( '\n', lineStart );
			const std::size_t end = ( lineEnd == std::string::npos ) ? text.size() : lineEnd;
			const char* p = text.c_str() + lineStart;
			const char* const e = text.c_str() + end;

			while( p < e && std::isspace( static_cast<unsigned char>( *p ) ) ){
				++p;
			}
			if( p < e && *p == '{' ){
				++depth;
				if( depth == 2 ){
					brushFaces.clear();
				}
			}
			else if( p < e && *p == '}' ){
				if( depth == 2 && !brushFaces.empty() ){
					append_brush_triangles( brushFaces, faces, points );
					brushFaces.clear();
				}
				if( depth > 0 ){
					--depth;
				}
			}
			else if( depth >= 2 && p < e && *p == '(' ){
				Vector3 a, b, c;
				const char* next = p;
				if( parse_vec3_at( next, a, &next )
				 && parse_vec3_at( next, b, &next )
				 && parse_vec3_at( next, c, &next ) ){
					while( next < e && std::isspace( static_cast<unsigned char>( *next ) ) ){
						++next;
					}
					const char* shaderBegin = next;
					while( next < e && !std::isspace( static_cast<unsigned char>( *next ) ) ){
						++next;
					}
					const std::string shader( shaderBegin, next );
					const Plane3 plane = plane3_for_points( a, b, c );
					if( plane3_valid( plane ) ){
						brushFaces.push_back( BrushFace{ plane, shader_color( shader ) } );
					}
				}
			}
			if( lineEnd == std::string::npos ){
				break;
			}
			lineStart = lineEnd + 1;
		}
	}

	QImage image( 256, 256, QImage::Format_ARGB32_Premultiplied );
	QLinearGradient gradient( 0, 0, 0, 256 );
	gradient.setColorAt( 0, QColor( 42, 46, 54 ) );
	gradient.setColorAt( 1, QColor( 30, 34, 41 ) );
	QPainter painter( &image );
	painter.fillRect( QRect( 0, 0, 256, 256 ), gradient );

	painter.setPen( QPen( QColor( 58, 64, 75 ), 1 ) );
	for( int y = 80; y < 256; y += 24 ){
		painter.drawLine( 0, y, 256, y );
	}

	if( !points.empty() ){
		Vector3 mins = points.front();
		Vector3 maxs = points.front();
		for( const Vector3& p : points ){
			mins[0] = std::min( mins[0], p[0] );
			mins[1] = std::min( mins[1], p[1] );
			mins[2] = std::min( mins[2], p[2] );
			maxs[0] = std::max( maxs[0], p[0] );
			maxs[1] = std::max( maxs[1], p[1] );
			maxs[2] = std::max( maxs[2], p[2] );
		}
		const Vector3 center = vector3_scaled( vector3_added( mins, maxs ), 0.5f );

		std::vector<QPointF> projected;
		projected.reserve( points.size() );
		qreal minX = 0, minY = 0, maxX = 0, maxY = 0;
		for( std::size_t i = 0; i < points.size(); ++i ){
			const ProjectedPoint qd = project_iso( vector3_subtracted( points[i], center ) );
			projected.push_back( qd.p );
			if( i == 0 ){
				minX = maxX = qd.p.x();
				minY = maxY = qd.p.y();
			}
			else{
				minX = std::min( minX, qd.p.x() );
				maxX = std::max( maxX, qd.p.x() );
				minY = std::min( minY, qd.p.y() );
				maxY = std::max( maxY, qd.p.y() );
			}
		}

		const qreal w = std::max<qreal>( maxX - minX, 1.0 );
		const qreal h = std::max<qreal>( maxY - minY, 1.0 );
		const qreal scale = std::min<qreal>( 184.0 / w, 148.0 / h );
		const qreal cx = ( minX + maxX ) * 0.5;
		const qreal cy = ( minY + maxY ) * 0.5;

		auto to_screen = [scale, cx, cy]( const QPointF& p ) -> QPointF {
			return QPointF( ( p.x() - cx ) * scale + 128.0, ( p.y() - cy ) * scale + 126.0 );
		};

		struct RenderTri
		{
			QPointF a, b, c;
			QColor color;
			float depth;
		};
		std::vector<RenderTri> renderTris;
		renderTris.reserve( faces.size() );
		for( const FacePreviewTri& face : faces ){
			const ProjectedPoint pa = project_iso( vector3_subtracted( face.a, center ) );
			const ProjectedPoint pb = project_iso( vector3_subtracted( face.b, center ) );
			const ProjectedPoint pc = project_iso( vector3_subtracted( face.c, center ) );
			const QPointF a = to_screen( pa.p );
			const QPointF b = to_screen( pb.p );
			const QPointF c = to_screen( pc.p );

			renderTris.push_back( { a, b, c, face.color, ( pa.depth + pb.depth + pc.depth ) / 3.f } );
		}

		std::sort( renderTris.begin(), renderTris.end(), []( const RenderTri& l, const RenderTri& r ){
			return l.depth < r.depth; // painter's algorithm: far to near
		} );

		painter.setRenderHint( QPainter::Antialiasing, true );
		for( const RenderTri& tri : renderTris ){
			QPolygonF poly;
			poly << tri.a << tri.b << tri.c;
			painter.setPen( Qt::NoPen );
			QColor c = tri.color;
			c.setAlpha( 90 );
			painter.setBrush( c );
			painter.drawPolygon( poly );
		}

		painter.setPen( QPen( QColor( 170, 220, 255, 170 ), 1.0 ) );
		for( const RenderTri& tri : renderTris ){
			painter.drawLine( tri.a, tri.b );
			painter.drawLine( tri.b, tri.c );
			painter.drawLine( tri.c, tri.a );
		}
	}

	const std::string stem = std::filesystem::path( mapPath ).stem().string();
	painter.setPen( QColor( 185, 220, 255 ) );
	painter.setFont( QFont( "Segoe UI", 14, QFont::Bold ) );
	painter.drawText( QRect( 16, 20, 224, 54 ), Qt::AlignLeft | Qt::AlignVCenter, "Prefab" );
	painter.setFont( QFont( "Segoe UI", 11 ) );
	painter.drawText( QRect( 16, 196, 224, 48 ), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, QString::fromUtf8( stem.c_str() ) );

	const QString qpath = QString::fromUtf8( previewPath.c_str() );
	QFileInfo info( qpath );
	if( !QDir().mkpath( info.path() ) ){
		return false;
	}
	return image.save( qpath, "PNG" );
}

QThreadPool& PrefabPreviewPool(){
	static QThreadPool pool;
	static bool init = false;
	if( !init ){
		init = true;
		pool.setMaxThreadCount( 1 );
		pool.setExpiryTimeout( 3000 );
	}
	return pool;
}

void PrefabBrowser_refreshPreviewByPath( const QString& previewPath ){
	if( g_prefabBrowser.m_list == nullptr || previewPath.isEmpty() ){
		return;
	}

	for( int i = 0; i < g_prefabBrowser.m_list->count(); ++i ){
		QListWidgetItem* item = g_prefabBrowser.m_list->item( i );
		if( item->data( Qt::ItemDataRole::UserRole + 1 ).toString() != previewPath ){
			continue;
		}
		if( !QFileInfo::exists( previewPath ) ){
			return;
		}
		item->setIcon( QIcon( previewPath ) );
		return;
	}
}

class PrefabPreviewTask final : public QRunnable
{
	std::string m_mapPath;
	std::string m_previewPath;
public:
	PrefabPreviewTask( std::string mapPath, std::string previewPath )
		: m_mapPath( std::move( mapPath ) ), m_previewPath( std::move( previewPath ) ){}

	void run() override {
		generate_preview_png( m_mapPath, m_previewPath );
		const QString path = QString::fromUtf8( m_previewPath.c_str() );
		QTimer::singleShot( 0, qApp, [path](){
			g_prefabBrowser.m_previewInFlight.erase( path.toUtf8().constData() );
			PrefabBrowser_refreshPreviewByPath( path );
		} );
	}
};

void PrefabBrowser_enqueuePreviewGeneration( const PrefabEntry& prefab ){
	if( file_exists( prefab.previewPath.c_str() ) ){
		return;
	}
	if( g_prefabBrowser.m_previewInFlight.find( prefab.previewPath ) != g_prefabBrowser.m_previewInFlight.end() ){
		return;
	}
	g_prefabBrowser.m_previewInFlight.insert( prefab.previewPath );

	auto* task = new PrefabPreviewTask( prefab.absolutePath, prefab.previewPath );
	task->setAutoDelete( true );
	PrefabPreviewPool().start( task, QThread::LowPriority );
}


std::string join_folder( const std::string& a, const std::string& b ){
	if( a.empty() ){
		return b;
	}
	if( b.empty() ){
		return a;
	}
	return StringStream( a.c_str(), "/", b.c_str() ).c_str();
}

std::vector<std::string> split_folder_parts( const std::string& folder ){
	std::vector<std::string> parts;
	std::size_t start = 0;
	while( start < folder.size() ){
		const std::size_t pos = folder.find( '/', start );
		if( pos == std::string::npos ){
			parts.push_back( folder.substr( start ) );
			break;
		}
		if( pos > start ){
			parts.push_back( folder.substr( start, pos - start ) );
		}
		start = pos + 1;
	}
	return parts;
}

void PrefabBrowser_updateInfo(){
	if( g_prefabBrowser.m_info == nullptr || g_prefabBrowser.m_list == nullptr ){
		return;
	}

	if( g_prefabBrowser.m_currentFolder.empty() ){
		g_prefabBrowser.m_info->setText( "Select folder in the tree" );
		return;
	}

	g_prefabBrowser.m_info->setText(
		QString( "%1 prefabs in %2" )
			.arg( g_prefabBrowser.m_list->count() )
			.arg( QString::fromUtf8( g_prefabBrowser.m_currentFolder.c_str() ) )
	);
}

void PrefabBrowser_applyPreviewSize(){
	if( g_prefabBrowser.m_list == nullptr ){
		return;
	}

	const int size = std::max( 64, std::min( 224, g_prefabBrowser.m_previewSize ) );
	const int cellW = size + 74;
	const int cellH = size + 34;
	g_prefabBrowser.m_list->setIconSize( QSize( size, size ) );
	g_prefabBrowser.m_list->setGridSize( QSize( cellW, cellH ) );
	for( int i = 0; i < g_prefabBrowser.m_list->count(); ++i ){
		if( QListWidgetItem* item = g_prefabBrowser.m_list->item( i ); item != nullptr ){
			item->setSizeHint( QSize( cellW, cellH ) );
			const QString previewPath = item->data( Qt::ItemDataRole::UserRole + 1 ).toString();
			if( previewPath.isEmpty() || !QFileInfo::exists( previewPath ) ){
				item->setIcon( PrefabBrowser_placeholderIcon( size ) );
			}
		}
	}
}

void PrefabBrowser_rebuildList(){
	if( g_prefabBrowser.m_list == nullptr ){
		return;
	}

	const QString filter = g_prefabBrowser.m_filter != nullptr
		? g_prefabBrowser.m_filter->text().trimmed()
		: QString();
	g_prefabBrowser.m_list->clear();
	for( const PrefabEntry& prefab : g_prefabBrowser.m_prefabs ){
		if( !should_show_prefab( prefab.modelPath, filter ) ){
			continue;
		}

		const int size = std::max( 64, std::min( 224, g_prefabBrowser.m_previewSize ) );
		QIcon icon = PrefabBrowser_placeholderIcon( size );
		const bool hasPreview = file_exists( prefab.previewPath.c_str() );
		if( hasPreview ){
			icon = QIcon( QString::fromUtf8( prefab.previewPath.c_str() ) );
		}
		else{
			PrefabBrowser_enqueuePreviewGeneration( prefab );
		}

		QListWidgetItem* item = new QListWidgetItem(
			icon,
			QString::fromUtf8( std::filesystem::path( prefab.absolutePath ).stem().string().c_str() )
		);
		item->setFlags( item->flags() | Qt::ItemFlag::ItemIsDragEnabled );
		item->setData( Qt::ItemDataRole::UserRole, QString::fromUtf8( prefab.modelPath.c_str() ) );
		item->setData( Qt::ItemDataRole::UserRole + 1, QString::fromUtf8( prefab.previewPath.c_str() ) );
		item->setToolTip( QString::fromUtf8( prefab.modelPath.c_str() ) );
		item->setSizeHint( QSize( size + 74, size + 34 ) );
		g_prefabBrowser.m_list->addItem( item );
	}

	PrefabBrowser_applyPreviewSize();
	PrefabBrowser_updateInfo();
}

void PrefabBrowser_clearList(){
	g_prefabBrowser.m_prefabs.clear();
	g_prefabBrowser.m_currentFolder.clear();
	if( g_prefabBrowser.m_list != nullptr ){
		g_prefabBrowser.m_list->clear();
	}
	PrefabBrowser_updateInfo();
}

void PrefabBrowser_loadFolder( const std::string& folderRel ){
	g_prefabBrowser.m_prefabs.clear();
	g_prefabBrowser.m_currentFolder = folderRel.empty() ? "maps" : folderRel;

	const std::string folderAbs = folderRel.empty()
		? StringStream( DirectoryCleaned( getMapsPath() ) ).c_str()
		: StringStream( DirectoryCleaned( getMapsPath() ), folderRel.c_str(), "/" ).c_str();

	std::error_code error;
	const std::filesystem::path folderPath( folderAbs );
	if( !std::filesystem::exists( folderPath, error ) || !std::filesystem::is_directory( folderPath, error ) ){
		PrefabBrowser_rebuildList();
		return;
	}

	for( std::filesystem::directory_iterator it( folderPath, error ), end; it != end; it.increment( error ) ){
		if( error ){
			error.clear();
			continue;
		}
		if( !it->is_regular_file() || !has_map_extension( it->path() ) ){
			continue;
		}

		PrefabEntry entry;
		entry.absolutePath = clean_path( it->path().string() );
		entry.modelPath = maps_relative_to_prefab_model( entry.absolutePath );
		std::filesystem::path preview = it->path();
		preview.replace_extension( ".preview.png" );
		entry.previewPath = clean_path( preview.string() );

		g_prefabBrowser.m_prefabs.emplace_back( std::move( entry ) );
	}

	std::sort(
		g_prefabBrowser.m_prefabs.begin(),
		g_prefabBrowser.m_prefabs.end(),
		[]( const PrefabEntry& a, const PrefabEntry& b ){
			return path_less( a.modelPath.c_str(), b.modelPath.c_str() );
		}
	);

	PrefabBrowser_rebuildList();
}

void PrefabBrowser_buildTree(){
	if( g_prefabBrowser.m_tree == nullptr ){
		return;
	}

	g_prefabBrowser.m_tree->clear();
	PrefabBrowser_clearList();

	std::set<std::string, PathStringLess> folderSet;
	folderSet.emplace( "" );

	const std::filesystem::path mapsRoot( getMapsPath() );
	std::error_code error;
	if( !std::filesystem::exists( mapsRoot, error ) || !std::filesystem::is_directory( mapsRoot, error ) ){
		return;
	}

	for( std::filesystem::recursive_directory_iterator it( mapsRoot, error ), end; it != end; it.increment( error ) ){
		if( error ){
			error.clear();
			continue;
		}
		if( !it->is_regular_file() || !has_map_extension( it->path() ) ){
			continue;
		}

		const std::string absFile = clean_path( it->path().string() );
		const std::string absFolder = StringStream( PathFilenameless( absFile.c_str() ) ).c_str();
		const char* relFolder = path_make_relative( absFolder.c_str(), getMapsPath() );
		if( relFolder == absFolder.c_str() ){
			continue;
		}
		folderSet.emplace( StringStream( PathCleaned( relFolder ) ).c_str() );
	}

	std::map<std::string, QTreeWidgetItem*, PathStringLess> items;
	QTreeWidgetItem* root = new QTreeWidgetItem( g_prefabBrowser.m_tree );
	root->setText( 0, "maps" );
	root->setData( 0, Qt::ItemDataRole::UserRole, QString() );
	items.emplace( "", root );

	for( const std::string& folder : folderSet ){
		if( folder.empty() ){
			continue;
		}

		const std::vector<std::string> parts = split_folder_parts( folder );
		std::string cur;
		for( const std::string& part : parts ){
			const std::string next = join_folder( cur, part );
			if( items.find( next ) == items.end() ){
				QTreeWidgetItem* parent = items.at( cur );
				QTreeWidgetItem* item = new QTreeWidgetItem( parent );
				item->setText( 0, QString::fromUtf8( part.c_str() ) );
				item->setData( 0, Qt::ItemDataRole::UserRole, QString::fromUtf8( next.c_str() ) );
				items.emplace( next, item );
			}
			cur = next;
		}
	}

	g_prefabBrowser.m_tree->collapseAll();
}

void PrefabBrowser_insertSelected(){
	if( g_prefabBrowser.m_list == nullptr ){
		return;
	}

	QListWidgetItem* item = g_prefabBrowser.m_list->currentItem();
	if( item == nullptr ){
		return;
	}

	const QString model = item->data( Qt::ItemDataRole::UserRole ).toString();
	if( model.isEmpty() ){
		return;
	}

	const QByteArray modelUtf8 = model.toUtf8();
	const Vector3 origin = prefab_insert_origin();
	Entity_createFromSelection( "misc_prefab", origin, modelUtf8.constData() );
}

void PrefabBrowser_onTreeActivated( QTreeWidgetItem* item ){
	if( item == nullptr ){
		PrefabBrowser_clearList();
		return;
	}
	const QString rel = item->data( 0, Qt::ItemDataRole::UserRole ).toString();
	PrefabBrowser_loadFolder( rel.toUtf8().constData() );
}
}

QWidget* PrefabBrowser_constructWindow( QWidget* toplevel ){
	if( g_prefabFallbackIcon.isNull() ){
		g_prefabFallbackIcon = QIcon( new_local_image( "prefab_preview.svg" ) );
	}
	auto* widget = new QWidget( toplevel );
	auto* vbox = new QVBoxLayout( widget );
	vbox->setContentsMargins( 4, 4, 4, 4 );

	auto* topBar = new QHBoxLayout;
	auto* filter = g_prefabBrowser.m_filter = new QLineEdit;
	filter->setPlaceholderText( "Filter prefabs..." );
	auto* refreshButton = new QPushButton( "Refresh" );
	auto* sizeLabel = new QLabel( "Preview:" );
	auto* sizeCombo = new QComboBox;
	sizeCombo->addItem( "S", 80 );
	sizeCombo->addItem( "M", 96 );
	sizeCombo->addItem( "L", 128 );
	sizeCombo->addItem( "XL", 160 );
	topBar->addWidget( filter, 1 );
	topBar->addWidget( refreshButton );
	topBar->addWidget( sizeLabel );
	topBar->addWidget( sizeCombo );
	vbox->addLayout( topBar );

	auto* splitter = new QSplitter;
	vbox->addWidget( splitter, 1 );

	auto* tree = g_prefabBrowser.m_tree = new QTreeWidget;
	tree->setHeaderHidden( true );
	tree->setEditTriggers( QAbstractItemView::EditTrigger::NoEditTriggers );
	tree->setUniformRowHeights( true );
	tree->header()->setStretchLastSection( false );
	tree->header()->setSectionResizeMode( QHeaderView::ResizeMode::ResizeToContents );
	splitter->addWidget( tree );

	auto* list = g_prefabBrowser.m_list = new PrefabListWidget;
	list->setSelectionMode( QAbstractItemView::SelectionMode::SingleSelection );
	list->setDragEnabled( true );
	list->setDragDropMode( QAbstractItemView::DragDropMode::DragOnly );
	list->setDefaultDropAction( Qt::DropAction::CopyAction );
	list->setViewMode( QListView::ViewMode::IconMode );
	list->setResizeMode( QListView::ResizeMode::Adjust );
	list->setMovement( QListView::Movement::Static );
	list->setWrapping( true );
	list->setWordWrap( true );
	list->setUniformItemSizes( true );
	list->setIconSize( QSize( 96, 96 ) );
	list->setGridSize( QSize( 170, 130 ) );
	splitter->addWidget( list );
	splitter->setStretchFactor( 0, 0 );
	splitter->setStretchFactor( 1, 1 );

	{
		const int preferredSize = std::max( 64, std::min( 224, g_prefabBrowser.m_previewSize ) );
		int matchIndex = sizeCombo->findData( preferredSize );
		if( matchIndex < 0 ){
			matchIndex = 1;
			g_prefabBrowser.m_previewSize = sizeCombo->itemData( matchIndex ).toInt();
		}
		sizeCombo->setCurrentIndex( matchIndex );
	}
	g_prefabBrowser.m_info = nullptr;

	QObject::connect( refreshButton, &QPushButton::clicked, PrefabBrowser_buildTree );
	QObject::connect( sizeCombo, QOverload<int>::of( &QComboBox::currentIndexChanged ), [sizeCombo]( int index ){
		g_prefabBrowser.m_previewSize = sizeCombo->itemData( index ).toInt();
		PrefabBrowser_applyPreviewSize();
	} );
	QObject::connect( filter, &QLineEdit::textChanged, []( const QString& ){ PrefabBrowser_rebuildList(); } );
	QObject::connect( list, &QListWidget::itemDoubleClicked, []( QListWidgetItem* ){ PrefabBrowser_insertSelected(); } );
	QObject::connect( tree, &QTreeWidget::itemActivated, []( QTreeWidgetItem* item, int ){ PrefabBrowser_onTreeActivated( item ); } );
	QObject::connect( tree, &QTreeWidget::itemClicked, []( QTreeWidgetItem* item, int ){ PrefabBrowser_onTreeActivated( item ); } );

	PrefabBrowser_applyPreviewSize();
	PrefabBrowser_buildTree();
	return widget;
}

void PrefabBrowser_destroyWindow(){
	g_prefabBrowser.m_tree = nullptr;
	g_prefabBrowser.m_filter = nullptr;
	g_prefabBrowser.m_list = nullptr;
	g_prefabBrowser.m_info = nullptr;
	g_prefabBrowser.m_prefabs.clear();
	g_prefabBrowser.m_currentFolder.clear();
	g_prefabBrowser.m_previewInFlight.clear();
}

void PrefabBrowser_Construct(){
	GlobalPreferenceSystem().registerPreference(
		"PrefabBrowserPreviewSize",
		IntImportStringCaller( g_prefabBrowser.m_previewSize ),
		IntExportStringCaller( g_prefabBrowser.m_previewSize )
	);
}

void PrefabBrowser_Destroy(){
}
