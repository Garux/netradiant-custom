#include "export.h"
#include "debugging/debugging.h"
#include "ibrush.h"
#include "iscenegraph.h"
#include "iselection.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"

#include <map>

// this is very evil, but right now there is no better way
#include "../../radiant/brush.h"

// for limNames
#define MAX_MATERIAL_NAME 20

/*
    Abstract baseclass for modelexporters
    the class collects all the data which then gets
    exported through the WriteToFile method.
 */
class ExportData
{
public:
ExportData( const StringSetWithLambda& ignorelist, collapsemode mode );
virtual ~ExportData( void );

virtual void BeginBrush( Brush& b );
virtual void AddBrushFace( Face& f );
virtual void EndBrush( void );

virtual bool WriteToFile( const std::string& path, collapsemode mode ) const = 0;

protected:

// a group of faces
class group
{
public:
std::string name;
std::list<const Face*> faces;
};

std::list<group> groups;

private:

// "textures/common/caulk" -> "caulk"
void GetShaderNameFromShaderPath( const char* path, std::string& name );

group* current;
const collapsemode mode;
const StringSetWithLambda& ignorelist;
};

ExportData::ExportData( const StringSetWithLambda& _ignorelist, collapsemode _mode )
	:   mode( _mode ),
	ignorelist( _ignorelist ){
	current = 0;

	// in this mode, we need just one group
	if ( mode == COLLAPSE_ALL ) {
		groups.push_back( group() );
		current = &groups.back();
		current->name = "all";
	}
}

ExportData::~ExportData( void ){

}

void ExportData::BeginBrush( Brush& b ){
	// create a new group for each brush
	if ( mode == COLLAPSE_NONE ) {
		groups.push_back( group() );
		current = &groups.back();

		StringOutputStream str( 256 );
		str << "Brush" << (const unsigned int)groups.size();
		current->name = str.c_str();
	}
}

void ExportData::EndBrush( void ){
	// all faces of this brush were on the ignorelist, discard the emptygroup
	if ( mode == COLLAPSE_NONE ) {
		ASSERT_NOTNULL( current );
		if ( current->faces.empty() ) {
			groups.pop_back();
			current = 0;
		}
	}
}

void ExportData::AddBrushFace( Face& f ){
	std::string shadername;
	GetShaderNameFromShaderPath( f.GetShader(), shadername );

	// ignore faces from ignore list
	if ( ignorelist.find( shadername ) != ignorelist.end() ) {
		return;
	}

	if ( mode == COLLAPSE_BY_MATERIAL ) {
		// find a group for this material
		current = 0;
		const std::list<group>::iterator end( groups.end() );
		for ( std::list<group>::iterator it( groups.begin() ); it != end; ++it )
		{
			if ( it->name == shadername ) {
				current = &( *it );
			}
		}

		// no group found, create one
		if ( !current ) {
			groups.push_back( group() );
			current = &groups.back();
			current->name = shadername;
		}
	}

	ASSERT_NOTNULL( current );

	// add face to current group
	current->faces.push_back( &f );

#ifdef _DEBUG
	globalOutputStream() << "Added Face to group " << current->name.c_str() << "\n";
#endif
}

void ExportData::GetShaderNameFromShaderPath( const char* path, std::string& name ){
	std::string tmp( path );

	size_t last_slash = tmp.find_last_of( "/" );

	if ( last_slash == std::string::npos || last_slash == ( tmp.length() - 1 ) ) {
		name = path;
	}
	else{
		name = tmp.substr( last_slash + 1, tmp.length() - last_slash );
	}

#ifdef _DEBUG
	globalOutputStream() << "Last: " << (const unsigned int) last_slash << " " << "length: " << (const unsigned int)tmp.length() << "Name: " << name.c_str() << "\n";
#endif
}

/*
    Exporter writing facedata as wavefront object
 */
class ExportDataAsWavefront : public ExportData
{
private:
const bool expmat;
const bool limNames;
const bool objs;
const bool weld;

public:
ExportDataAsWavefront( const StringSetWithLambda& _ignorelist, collapsemode _mode, bool _expmat, bool _limNames, bool _objs, bool _weld )
	: ExportData( _ignorelist, _mode ),
	expmat( _expmat ),
	limNames( _limNames ),
	objs( _objs ),
	weld( _weld ){
}

bool WriteToFile( const std::string& path, collapsemode mode ) const;
};

bool ExportDataAsWavefront::WriteToFile( const std::string& path, collapsemode mode ) const {
	std::string objFile = path;

	if ( !string_equal_suffix_nocase( objFile.c_str(), ".obj" ) ) {
		objFile += ".obj";
	}

	std::string mtlFile = objFile.substr( 0, objFile.length() - 4 ) + ".mtl";

	auto materials_comparator = []( const std::string& lhs, const std::string& rhs ) {
		return lhs < rhs;
//		return string_less_nocase( lhs.c_str(), rhs.c_str() ); // can't squash varying cases just here, as usemtl is case sensitive in blender
	};
	auto materials = std::map<std::string, Colour3, decltype( materials_comparator )>( materials_comparator );

	TextFileOutputStream out( objFile.c_str() );

	if ( out.failed() ) {
		globalErrorStream() << "Unable to open file\n";
		return false;
	}

	out << "# Wavefront Objectfile exported with radiants brushexport plugin 3.0 by Thomas 'namespace' Nitschke, spam@codecreator.net\n\n";

	if ( expmat ) {
		size_t last = mtlFile.find_last_of( "//" );
		std::string mtllib = mtlFile.substr( last + 1, mtlFile.size() - last ).c_str();
		out << "mtllib " << mtllib.c_str() << "\n";
	}

	unsigned int vertex_count = 0;
	unsigned int texcoord_count = 0;

	for ( const ExportData::group& group : groups )
	{
		std::multimap<std::string, std::string> brushMaterials;

		std::vector<Vector3> vertices; // unique vertices list for welding

		// submesh starts here
		if ( objs ) {
			out << "\no ";
		}
		else {
			out << "\ng ";
		}
		out << group.name.c_str() << "\n";

		// material
		if ( expmat && mode == COLLAPSE_ALL ) {
			out << "usemtl material" << "\n\n";
			materials.emplace( "material", Colour3( 0.5, 0.5, 0.5 ) );
		}

		for ( const Face* face : group.faces )
		{
			const Winding& w( face->getWinding() );

			// faces
			StringOutputStream faceLine( 256 );
			faceLine << "\nf";

			size_t i = w.numpoints;
			do{
				--i;
				++texcoord_count;
				std::size_t vertexN = 0; // vertex index to use, 0 is special value = no vertex to weld to found
				const Vector3& vertex = w[i].vertex;
				if( weld ){
					auto found = std::find_if( vertices.begin(), vertices.end(), [&vertex]( const Vector3& othervertex ){
										return Edge_isDegenerate( vertex, othervertex );
									} );
					if( found == vertices.end() ){ // unique vertex, add to the list
						vertices.emplace_back( vertex );
					}
					else{
						vertexN = vertex_count - std::distance( found, vertices.end() ) + 1; // reuse existing index
					}
				}
				// write vertices
				if( vertexN == 0 ){
					vertexN = ++vertex_count;
					out << "v " << FloatFormat( vertex.x(), 1, 6 ) << " " << FloatFormat( vertex.z(), 1, 6 ) << " " << FloatFormat( -vertex.y(), 1, 6 ) << "\n";
				}
				faceLine << " " << vertexN << "/" << texcoord_count; // store faces
			}
			while( i != 0 );

			if ( mode != COLLAPSE_ALL )
				materials.emplace( face->getShader().getShader(), face->getShader().state()->getTexture().color );

			brushMaterials.emplace( face->getShader().getShader(), faceLine.c_str() );
		}

		out << "\n";

		for ( const Face* face : group.faces )
		{
			const Winding& w( face->getWinding() );

			// texcoords
			size_t i = w.numpoints;
			do{
				--i;
				out << "vt " << FloatFormat( w[i].texcoord.x(), 1, 6 ) << " " << FloatFormat( -w[i].texcoord.y(), 1, 6 ) << "\n";
			}
			while( i != 0 );
		}

		{
			std::string lastMat;
			for ( const auto& stringpair : brushMaterials )
			{
				const std::string& mat = stringpair.first;
				const std::string& faces = stringpair.second;

				if ( mode != COLLAPSE_ALL && mat != lastMat ) {
					if ( limNames && mat.size() > MAX_MATERIAL_NAME ) {
						out << "\nusemtl " << mat.substr( mat.size() - MAX_MATERIAL_NAME, mat.size() ).c_str();
					}
					else {
						out << "\nusemtl " << mat.c_str();
					}
					lastMat = mat;
				}
				// write faces
				out << faces.c_str();
			}
		}

		out << "\n";
	}

	if ( expmat ) {
		TextFileOutputStream outMtl( mtlFile.c_str() );
		if ( outMtl.failed() ) {
			globalErrorStream() << "Unable to open material file\n";
			return false;
		}

		outMtl << "# Wavefront material file exported with NetRadiants brushexport plugin.\n";
		outMtl << "# Material Count: " << (const Unsigned)materials.size() << "\n\n";
		for ( const auto& material : materials )
		{
			const std::string& str = material.first;
			const Colour3& clr = material.second;
			if ( limNames && str.size() > MAX_MATERIAL_NAME ) {
				outMtl << "newmtl " << str.substr( str.size() - MAX_MATERIAL_NAME, str.size() ).c_str() << "\n";
			}
			else {
				outMtl << "newmtl " << str.c_str() << "\n";
			}
			outMtl << "Kd " << clr.x() << " " << clr.y() << " " << clr.z() << "\n";
			outMtl << "map_Kd " << str.c_str() << "\n";

		}
	}

	return true;
}


class ForEachFace : public BrushVisitor
{
public:
ForEachFace( ExportData& _exporter )
	: exporter( _exporter )
{}

void visit( Face& face ) const {
	if( face.contributes() )
		exporter.AddBrushFace( face );
}

private:
ExportData& exporter;
};

class ForEachSelected : public SelectionSystem::Visitor
{
public:
ForEachSelected( ExportData& _exporter )
	: exporter( _exporter )
{}

void visit( scene::Instance& instance ) const {
	BrushInstance* bptr = InstanceTypeCast<BrushInstance>::cast( instance );
	if ( bptr ) {
		Brush& brush( bptr->getBrush() );

		exporter.BeginBrush( brush );
		ForEachFace face_vis( exporter );
		brush.forEachFace( face_vis );
		exporter.EndBrush();
	}
}

private:
ExportData& exporter;
};

#include "plugin.h"
#include "qerplugin.h"

bool ExportSelection( const StringSetWithLambda& ignorelist, collapsemode m, bool exmat, const std::string& path, bool limNames, bool objs, bool weld ){
	ExportDataAsWavefront exporter( ignorelist, m, exmat, limNames, objs, weld );

	if( GlobalSelectionSystem().countSelected() == 0 ){
		globalErrorStream() << "Nothing is selected.\n";
		GlobalRadiant().m_pfnMessageBox( g_pRadiantWnd, "Nothing is selected.", "brushexport", eMB_OK, eMB_ICONERROR );
		return false;
	}

	ForEachSelected vis( exporter );
	GlobalSelectionSystem().foreachSelected( vis );

	if( exporter.WriteToFile( path, m ) ){
		globalOutputStream() << "brushexport::ExportSelection " << path.c_str() << "\n";
		return true;
	}

	return false;
}
