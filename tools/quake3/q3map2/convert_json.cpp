/* -------------------------------------------------------------------------------

   Copyright (C) 1999-2007 id Software, Inc. and contributors.
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

   -------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* dependencies */
#include "q3map2.h"
#define RAPIDJSON_WRITE_DEFAULT_FLAGS kWriteNanAndInfFlag
#define RAPIDJSON_PARSE_DEFAULT_FLAGS ( kParseCommentsFlag | kParseTrailingCommasFlag | kParseNanAndInfFlag )
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

#include <map>

#define for_indexed(...) for_indexed_v(i, __VA_ARGS__)
#define for_indexed_v(v, ...) if (std::size_t v = -1) for (__VA_ARGS__) if ((++v, true))


template<typename T, typename Allocator>
rapidjson::Value value_for( const BasicVector2<T>& vec, Allocator& allocator ){
	rapidjson::Value value( rapidjson::kArrayType );
	for( size_t i = 0; i != 2; ++i )
		value.PushBack( vec[i], allocator );
	return value;
}
template<typename T, typename Allocator>
rapidjson::Value value_for( const BasicVector3<T>& vec, Allocator& allocator ){
	rapidjson::Value value( rapidjson::kArrayType );
	for( size_t i = 0; i != 3; ++i )
		value.PushBack( vec[i], allocator );
	return value;
}
template<typename T, typename Allocator>
rapidjson::Value value_for( const BasicVector4<T>& vec, Allocator& allocator ){
	rapidjson::Value value( rapidjson::kArrayType );
	for( size_t i = 0; i != 4; ++i )
		value.PushBack( vec[i], allocator );
	return value;
}
template<typename T, size_t N, typename Allocator>
rapidjson::Value value_for( const T (&array)[N], Allocator& allocator ){
	rapidjson::Value value( rapidjson::kArrayType );
	for( auto&& val : array )
		value.PushBack( value_for( val, allocator ), allocator );
	return value;
}
template<typename T, size_t N, typename Allocator>
rapidjson::Value value_for_array( const T (&array)[N], Allocator& allocator ){
	rapidjson::Value value( rapidjson::kArrayType );
	for( auto&& val : array )
		value.PushBack( val, allocator );
	return value;
}

template<typename T>
void value_to( const rapidjson::Value& value, BasicVector2<T>& vec ){
	for( size_t i = 0; i != 2; ++i )
		vec[i] = value.GetArray().operator[]( i ).Get<T>();
}
template<typename T>
void value_to( const rapidjson::Value& value, BasicVector3<T>& vec ){
	for( size_t i = 0; i != 3; ++i )
		if constexpr ( std::is_same_v<T, byte> )
			vec[i] = value.GetArray().operator[]( i ).Get<int>();
		else
			vec[i] = value.GetArray().operator[]( i ).Get<T>();
}
template<typename T>
void value_to( const rapidjson::Value& value, BasicVector4<T>& vec ){
	for( size_t i = 0; i != 4; ++i )
		if constexpr ( std::is_same_v<T, byte> )
			vec[i] = value.GetArray().operator[]( i ).Get<int>();
		else
			vec[i] = value.GetArray().operator[]( i ).Get<T>();
}
template<typename T, size_t N>
void value_to( const rapidjson::Value& value, T (&array)[N] ){
	for_indexed( auto&& val : array )
		value_to( value.GetArray().operator[]( i ), val );
}
template<typename T, size_t N>
void value_to_array( const rapidjson::Value& value, T (&array)[N] ){
	for_indexed( auto&& val : array ){
		if constexpr ( std::is_same_v<T, byte> )
			val = value.GetArray().operator[]( i ).Get<int>();
		else
			val = value.GetArray().operator[]( i ).Get<T>();
	}
}


static void write_doc( const char *filename, rapidjson::Document& doc ){
	rapidjson::StringBuffer buffer;
   	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer( buffer );
	writer.SetFormatOptions( rapidjson::kFormatSingleLineArray );
   	doc.Accept( writer );
	Sys_Printf( "Writing %s\n", filename );
	SaveFile( filename, buffer.GetString(), buffer.GetSize() );
}

static void write_json( const char *directory ){
	Q_mkdir( directory );

	rapidjson::Document doc( rapidjson::kObjectType );
	auto& all = doc.GetAllocator();

	{
		doc.RemoveAllMembers();
		for_indexed( const auto& shader : bspShaders ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "shader", rapidjson::StringRef( shader.shader ), all );
			{
				int flags = shader.surfaceFlags;
				std::map<uint32_t, const char*> map; // decompose flags
				for( auto parm = g_game->surfaceParms.crbegin(); parm != g_game->surfaceParms.crend(); ++parm  ){ // find known flags // traverse backwards to try fatter material flags 1st
					if( parm->surfaceFlags == ( flags & parm->surfaceFlags ) && parm->surfaceFlags != 0 ){
						flags &= ~parm->surfaceFlags;
						map.emplace( parm->surfaceFlags, parm->name );
					}
				}
				for( int bit = 0; bit < 32; ++bit ){ // handle unknown flags
					if( flags & ( 1 << bit ) ){
						map.emplace( ( 1 << bit ), "?" );
					}
				}
				rapidjson::Value fvalue( rapidjson::kObjectType );
				for( auto&& [ flag, name ] : map ){ // save in hex format
					char c[16];
					sprintf( c, "%#.8x", flag );
					fvalue.AddMember( rapidjson::StringRef( name ), rapidjson::Value( c, all ), all );
				}
				value.AddMember( "surfaceFlags", fvalue, all );
			}
			{
				int flags = shader.contentFlags;
				std::map<uint32_t, const char*> map; // decompose flags
				for( auto parm = g_game->surfaceParms.crbegin(); parm != g_game->surfaceParms.crend(); ++parm  ){ // find known flags // traverse backwards to try fatter material flags 1st
					if( parm->contentFlags == ( flags & parm->contentFlags ) && parm->contentFlags != 0 ){
						flags &= ~parm->contentFlags;
						map.emplace( parm->contentFlags, parm->name );
					}
				}
				for( int bit = 0; bit < 32; ++bit ){ // handle unknown flags
					if( flags & ( 1 << bit ) ){
						map.emplace( ( 1 << bit ), "?" );
					}
				}
				rapidjson::Value fvalue( rapidjson::kObjectType );
				for( auto&& [ flag, name ] : map ){ // save in hex format
					char c[16];
					sprintf( c, "%#.8x", flag );
					fvalue.AddMember( rapidjson::StringRef( name ), rapidjson::Value( c, all ), all );
				}
				value.AddMember( "contentFlags", fvalue, all );
			}
			doc.AddMember( rapidjson::Value( StringStream<16>( "shader#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "shaders.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& model : bspModels ){
			rapidjson::Value value( rapidjson::kObjectType );
			{
				rapidjson::Value minmax( rapidjson::kObjectType );
				minmax.AddMember( "mins", value_for( model.minmax.mins, all ), all );
				minmax.AddMember( "maxs", value_for( model.minmax.maxs, all ), all );
				value.AddMember( "minmax", minmax, all );
			}
			value.AddMember( "firstBSPSurface", model.firstBSPSurface, all );
			value.AddMember( "numBSPSurfaces", model.numBSPSurfaces, all );
			value.AddMember( "firstBSPBrush", model.firstBSPBrush, all );
			value.AddMember( "numBSPBrushes", model.numBSPBrushes, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "model#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "models.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& plane : bspPlanes ){
			rapidjson::Value value( rapidjson::kArrayType );
			value.PushBack( plane.a, all );
			value.PushBack( plane.b, all );
			value.PushBack( plane.c, all );
			value.PushBack( plane.d, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "plane#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "planes.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& leaf : bspLeafs ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "cluster", leaf.cluster, all );
			value.AddMember( "area", leaf.area, all );
			{
				rapidjson::Value minmax( rapidjson::kObjectType );
				minmax.AddMember( "mins", value_for( leaf.minmax.mins, all ), all );
				minmax.AddMember( "maxs", value_for( leaf.minmax.maxs, all ), all );
				value.AddMember( "minmax", minmax, all );
			}
			value.AddMember( "firstBSPLeafSurface", leaf.firstBSPLeafSurface, all );
			value.AddMember( "numBSPLeafSurfaces", leaf.numBSPLeafSurfaces, all );
			value.AddMember( "firstBSPLeafBrush", leaf.firstBSPLeafBrush, all );
			value.AddMember( "numBSPLeafBrushes", leaf.numBSPLeafBrushes, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "leaf#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "leafs.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& node : bspNodes ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "planeNum", node.planeNum, all );
			value.AddMember( "children", value_for_array( node.children, all ), all );
			{
				rapidjson::Value minmax( rapidjson::kObjectType );
				minmax.AddMember( "mins", value_for( node.minmax.mins, all ), all );
				minmax.AddMember( "maxs", value_for( node.minmax.maxs, all ), all );
				value.AddMember( "minmax", minmax, all );
			}
			doc.AddMember( rapidjson::Value( StringStream<16>( "node#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "nodes.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& ls : bspLeafSurfaces ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "Num", ls, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "LeafSurface#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "LeafSurfaces.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& lb : bspLeafBrushes ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "Num", lb, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "LeafBrush#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "LeafBrushes.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& brush : bspBrushes ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "firstSide", brush.firstSide, all );
			value.AddMember( "numSides", brush.numSides, all );
			value.AddMember( "shaderNum", brush.shaderNum, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "Brush#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "Brushes.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& side : bspBrushSides ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "planeNum", side.planeNum, all );
			value.AddMember( "shaderNum", side.shaderNum, all );
			value.AddMember( "surfaceNum", side.surfaceNum, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "BrushSide#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "BrushSides.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& vert : bspDrawVerts ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "xyz", value_for( vert.xyz, all ), all );
			value.AddMember( "st", value_for( vert.st, all ), all );
			value.AddMember( "lightmap", value_for( vert.lightmap, all ), all );
			value.AddMember( "normal", value_for( vert.normal, all ), all );
			value.AddMember( "color", value_for( vert.color, all ), all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "DrawVert#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "DrawVert.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& surf : bspDrawSurfaces ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "shaderNum", surf.shaderNum, all );
			value.AddMember( "fogNum", surf.fogNum, all );
			value.AddMember( "surfaceType", surf.surfaceType, all ); /////////////////!!!!!!!!!! text?
			value.AddMember( "firstVert", surf.firstVert, all );
			value.AddMember( "numVerts", surf.numVerts, all );
			value.AddMember( "firstIndex", surf.firstIndex, all );
			value.AddMember( "numIndexes", surf.numIndexes, all );
			value.AddMember( "lightmapStyles", value_for_array( surf.lightmapStyles, all ), all );
			value.AddMember( "vertexStyles", value_for_array( surf.vertexStyles, all ), all );
			value.AddMember( "lightmapNum", value_for_array( surf.lightmapNum, all ), all );
			value.AddMember( "lightmapX", value_for_array( surf.lightmapX, all ), all );
			value.AddMember( "lightmapY", value_for_array( surf.lightmapY, all ), all );
			value.AddMember( "lightmapWidth", surf.lightmapWidth, all );
			value.AddMember( "lightmapHeight", surf.lightmapHeight, all );
			value.AddMember( "lightmapOrigin", value_for( surf.lightmapOrigin, all ), all );
			value.AddMember( "lightmapVecs", value_for( surf.lightmapVecs, all ), all );
			value.AddMember( "patchWidth", surf.patchWidth, all );
			value.AddMember( "patchHeight", surf.patchHeight, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "DrawSurface#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "DrawSurfaces.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& fog : bspFogs ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "shader", rapidjson::StringRef( fog.shader ), all );
			value.AddMember( "brushNum", fog.brushNum, all );
			value.AddMember( "visibleSide", fog.visibleSide, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "fog#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "fogs.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& index : bspDrawIndexes ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "Num", index, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "DrawIndex#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "DrawIndexes.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& index : bspVisBytes ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "Num", index, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "VisByte#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "VisBytes.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& index : bspLightBytes ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "Num", index, all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "LightByte#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "LightBytes.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( auto&& entity : entities ){
			rapidjson::Value value( rapidjson::kObjectType );
			for( auto&& kv : entity.epairs ){
				rapidjson::Value array( rapidjson::kArrayType );
				array.PushBack( rapidjson::StringRef( kv.key.c_str() ), all );
				array.PushBack( rapidjson::StringRef( kv.value.c_str() ), all );
				value.AddMember( "KeyValue", array, all );
			}
			doc.AddMember( rapidjson::Value( StringStream<16>( "entity#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "entities.json" ), doc );
	}
	{
		doc.RemoveAllMembers();
		for_indexed( const auto& point : bspGridPoints ){
			rapidjson::Value value( rapidjson::kObjectType );
			value.AddMember( "ambient", value_for( point.ambient, all ), all );
			value.AddMember( "directed", value_for( point.directed, all ), all );
			value.AddMember( "styles", value_for_array( point.styles, all ), all );
			value.AddMember( "latLong", value_for_array( point.latLong, all ), all );
			doc.AddMember( rapidjson::Value( StringStream<16>( "GridPoint#", i ).c_str(), all ), value, all );
		}
		write_doc( StringStream( directory, "GridPoints.json" ), doc );
	}
}

inline rapidjson::Document load_json( const char *fileName ){
	Sys_Printf( "Loading %s\n", fileName );
	rapidjson::Document doc;
	doc.Parse( (const char*)LoadFile( fileName ).data() );
	ENSURE( !doc.HasParseError() );
	return doc;
}

static void read_json( const char *directory, bool useFlagNames, bool skipUnknownFlags ){
	{
		const auto doc = load_json( StringStream( directory, "shaders.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspShaders.emplace_back();
			strcpy( item.shader, obj.value["shader"].GetString() );
			for( auto&& flag : obj.value["surfaceFlags"].GetObj() ){
				auto name = flag.name.GetString();
				auto value = flag.value.GetString();
				if( useFlagNames ){
					auto&& parms = g_game->surfaceParms;
					auto found = std::find_if( parms.begin(), parms.end(), [name]( const auto& parm ){ return striEqual( parm.name, name ); } );
					if( found != parms.end() ){
						item.surfaceFlags |= found->surfaceFlags;
					}
					else if( skipUnknownFlags ){
						Sys_Warning( "Flag name '%s' is unknown in shader '%s', skipping its value (%s)\n", name, item.shader, value );
					}
					else{
						Sys_Warning( "Flag name '%s' is unknown in shader '%s', using its value (%s)\n", name, item.shader, value );
						item.surfaceFlags |= strtoul( value, nullptr, 0 );
					}
				}
				else
					item.surfaceFlags |= strtoul( value, nullptr, 0 );
			}
			for( auto&& flag : obj.value["contentFlags"].GetObj() ){
				auto name = flag.name.GetString();
				auto value = flag.value.GetString();
				if( useFlagNames ){
					auto&& parms = g_game->surfaceParms;
					auto found = std::find_if( parms.begin(), parms.end(), [name]( const auto& parm ){ return striEqual( parm.name, name ); } );
					if( found != parms.end() ){
						item.contentFlags |= found->contentFlags;
					}
					else if( skipUnknownFlags ){
						Sys_Warning( "Flag name '%s' is unknown in shader '%s', skipping its value (%s)\n", name, item.shader, value );
					}
					else{
						Sys_Warning( "Flag name '%s' is unknown in shader '%s', using its value (%s)\n", name, item.shader, value );
						item.contentFlags |= strtoul( value, nullptr, 0 );
					}
				}
				else
					item.contentFlags |= strtoul( value, nullptr, 0 );
			}
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "models.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspModels.emplace_back();
			value_to( obj.value["minmax"].GetObj().operator[]( "mins" ), item.minmax.mins );
			value_to( obj.value["minmax"].GetObj().operator[]( "maxs" ), item.minmax.maxs );
			item.firstBSPSurface = obj.value["firstBSPSurface"].GetInt();
			item.numBSPSurfaces = obj.value["numBSPSurfaces"].GetInt();
			item.firstBSPBrush = obj.value["firstBSPBrush"].GetInt();
			item.numBSPBrushes = obj.value["numBSPBrushes"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "planes.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspPlanes.emplace_back();
			item.a = obj.value[0].GetFloat();
			item.b = obj.value[1].GetFloat();
			item.c = obj.value[2].GetFloat();
			item.d = obj.value[3].GetFloat();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "leafs.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspLeafs.emplace_back();
			item.cluster = obj.value["cluster"].GetInt();
			item.area = obj.value["area"].GetInt();
			value_to( obj.value["minmax"].GetObj().operator[]( "mins" ), item.minmax.mins );
			value_to( obj.value["minmax"].GetObj().operator[]( "maxs" ), item.minmax.maxs );
			item.firstBSPLeafSurface = obj.value["firstBSPLeafSurface"].GetInt();
			item.numBSPLeafSurfaces = obj.value["numBSPLeafSurfaces"].GetInt();
			item.firstBSPLeafBrush = obj.value["firstBSPLeafBrush"].GetInt();
			item.numBSPLeafBrushes = obj.value["numBSPLeafBrushes"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "nodes.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspNodes.emplace_back();
			item.planeNum = obj.value["planeNum"].GetInt();
			item.children[0] = obj.value["children"].operator[]( 0 ).GetInt();
			item.children[1] = obj.value["children"].operator[]( 1 ).GetInt();
			value_to( obj.value["minmax"].GetObj().operator[]( "mins" ), item.minmax.mins );
			value_to( obj.value["minmax"].GetObj().operator[]( "maxs" ), item.minmax.maxs );
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "LeafSurfaces.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspLeafSurfaces.emplace_back();
			item = obj.value["Num"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "LeafBrushes.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspLeafBrushes.emplace_back();
			item = obj.value["Num"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "Brushes.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspBrushes.emplace_back();
			item.firstSide = obj.value["firstSide"].GetInt();
			item.numSides = obj.value["numSides"].GetInt();
			item.shaderNum = obj.value["shaderNum"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "BrushSides.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspBrushSides.emplace_back();
			item.planeNum = obj.value["planeNum"].GetInt();
			item.shaderNum = obj.value["shaderNum"].GetInt();
			item.surfaceNum = obj.value["surfaceNum"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "DrawVert.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspDrawVerts.emplace_back();
			value_to( obj.value["xyz"], item.xyz );
			value_to( obj.value["st"], item.st );
			value_to( obj.value["lightmap"], item.lightmap );
			value_to( obj.value["normal"], item.normal );
			value_to( obj.value["color"], item.color );
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "DrawSurfaces.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspDrawSurfaces.emplace_back();
			item.shaderNum = obj.value["shaderNum"].GetInt();
			item.fogNum = obj.value["fogNum"].GetInt();
			item.surfaceType = obj.value["surfaceType"].GetInt();
			item.firstVert = obj.value["firstVert"].GetInt();
			item.numVerts = obj.value["numVerts"].GetInt();
			item.firstIndex = obj.value["firstIndex"].GetInt();
			item.numIndexes = obj.value["numIndexes"].GetInt();
			value_to_array( obj.value["lightmapStyles"], item.lightmapStyles );
			value_to_array( obj.value["vertexStyles"], item.vertexStyles );
			value_to_array( obj.value["lightmapNum"], item.lightmapNum );
			value_to_array( obj.value["lightmapX"], item.lightmapX );
			value_to_array( obj.value["lightmapY"], item.lightmapY );
			item.lightmapWidth = obj.value["lightmapWidth"].GetInt();
			item.lightmapHeight = obj.value["lightmapHeight"].GetInt();
			value_to( obj.value["lightmapOrigin"], item.lightmapOrigin );
			value_to( obj.value["lightmapVecs"], item.lightmapVecs );
			item.patchWidth = obj.value["patchWidth"].GetInt();
			item.patchHeight = obj.value["patchHeight"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "fogs.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspFogs.emplace_back();
			strcpy( item.shader, obj.value["shader"].GetString() );
			item.brushNum = obj.value["brushNum"].GetInt();
			item.visibleSide = obj.value["visibleSide"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "DrawIndexes.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspDrawIndexes.emplace_back();
			item = obj.value["Num"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "VisBytes.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspVisBytes.emplace_back();
			item = obj.value["Num"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "LightBytes.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspLightBytes.emplace_back();
			item = obj.value["Num"].GetInt();
		}
	}
	{
		const auto doc = load_json( StringStream( directory, "entities.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& entity = entities.emplace_back();
			for( auto&& kvobj : obj.value.GetObj() ){
				auto&& kv = entity.epairs.emplace_back();
				kv.key = kvobj.value[0].GetString();
				kv.value = kvobj.value[1].GetString();
			}
		}
		numBSPEntities = entities.size();
	}
	{
		const auto doc = load_json( StringStream( directory, "GridPoints.json" ) );
		for( auto&& obj : doc.GetObj() ){
			auto&& item = bspGridPoints.emplace_back();
			value_to( obj.value["ambient"], item.ambient );
			value_to( obj.value["directed"], item.directed );
			value_to_array( obj.value["styles"], item.styles );
			value_to_array( obj.value["latLong"], item.latLong );
		}
	}
}

int ConvertJsonMain( Args& args ){
	/* arg checking */
	if ( args.empty() ) {
		Sys_Printf( "Usage: q3map2 -json <-unpack|-pack [-useflagnames[-skipflags]]> [-v] <mapname>\n" );
		return 0;
	}

	bool doPack = false; // unpack by default
	bool useFlagNames = false;
	bool skipUnknownFlags = false;

	/* process arguments */
	const char *fileName = args.takeBack();
	{
		while ( args.takeArg( "-pack" ) ) {
			doPack = true;
		}
		while ( args.takeArg( "-useflagnames" ) ) {
			useFlagNames = true;
			Sys_Printf( "Deducing surface/content flag values from their names in shaders.json\n" );
		}
		while ( args.takeArg( "-skipflags" ) ) {
			skipUnknownFlags = true;
			Sys_Printf( "Skipping unknown surface/content flag names\n" );
		}
	}

	// LoadShaderInfo();

	/* clean up map name */
	strcpy( source, ExpandArg( fileName ) );

	if( !doPack ){ // unpack
		path_set_extension( source, ".bsp" );
		Sys_Printf( "Loading %s\n", source );
		LoadBSPFile( source );
		ParseEntities();
		write_json( StringStream( PathExtensionless( source ), '/' ) );
	}
	else{
		/* write bsp */
		read_json( StringStream( PathExtensionless( source ), '/' ), useFlagNames, skipUnknownFlags );
		UnparseEntities();
		path_set_extension( source, "_json.bsp" );
		WriteBSPFile( source );
	}

	/* return to sender */
	return 0;
}
