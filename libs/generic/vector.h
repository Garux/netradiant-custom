
#pragma once

#include <cstddef>

template <typename Element>
class BasicVector2
{
	Element m_elements[2];
public:
	BasicVector2(){
	}
	template<typename OtherElement>
	BasicVector2( const BasicVector2<OtherElement>& other ) : m_elements{
		static_cast<Element>( other.x() ),
		static_cast<Element>( other.y() ) }
	{}
	BasicVector2( const Element& x_, const Element& y_ ) : m_elements{ x_, y_ }{
	}
	explicit BasicVector2( const Element& value ) : m_elements{ value, value }{
	}

	Element& x(){
		return m_elements[0];
	}
	const Element& x() const {
		return m_elements[0];
	}
	Element& y(){
		return m_elements[1];
	}
	const Element& y() const {
		return m_elements[1];
	}

	const Element& operator[]( std::size_t i ) const {
		return m_elements[i];
	}
	Element& operator[]( std::size_t i ){
		return m_elements[i];
	}

	Element* data(){
		return m_elements;
	}
	const Element* data() const {
		return m_elements;
	}
};

/// \brief A 3-element vector.
template<typename Element>
class BasicVector3
{
	Element m_elements[3];
public:

	BasicVector3(){
	}
	template<typename OtherElement>
	BasicVector3( const BasicVector3<OtherElement>& other ) : m_elements{
		static_cast<Element>( other.x() ),
		static_cast<Element>( other.y() ),
		static_cast<Element>( other.z() ) }
	{}
	template<typename OtherElement>
	explicit BasicVector3( const BasicVector2<OtherElement>& vec2, const Element& z_ ) : m_elements{
		static_cast<Element>( vec2.x() ),
		static_cast<Element>( vec2.y() ),
		z_ }
	{}
	BasicVector3( const Element& x_, const Element& y_, const Element& z_ ) : m_elements{ x_, y_, z_ }{
	}
	explicit BasicVector3( const Element& value ) : m_elements{ value, value, value }{
	}

	Element& x(){
		return m_elements[0];
	}
	const Element& x() const {
		return m_elements[0];
	}
	Element& y(){
		return m_elements[1];
	}
	const Element& y() const {
		return m_elements[1];
	}
	Element& z(){
		return m_elements[2];
	}
	const Element& z() const {
		return m_elements[2];
	}

	const Element& operator[]( std::size_t i ) const {
		return m_elements[i];
	}
	Element& operator[]( std::size_t i ){
		return m_elements[i];
	}

	Element* data(){
		return m_elements;
	}
	const Element* data() const {
		return m_elements;
	}

	BasicVector2<Element>& vec2(){
		return reinterpret_cast<BasicVector2<Element>&>( x() );
	}
	const BasicVector2<Element>& vec2() const {
		return reinterpret_cast<const BasicVector2<Element>&>( x() );
	}

	void set( const Element value ){
		x() = y() = z() = value;
	}
};

/// \brief A 4-element vector.
template<typename Element>
class BasicVector4
{
	Element m_elements[4];
public:

	BasicVector4(){
	}
	BasicVector4( Element x_, Element y_, Element z_, Element w_ ) : m_elements{ x_, y_, z_, w_ }{
	}
	BasicVector4( const BasicVector3<Element>& vec3, Element w_ ) : m_elements{
		vec3.x(),
		vec3.y(),
		vec3.z(),
		w_ }
	{}
	explicit BasicVector4( const Element& value ) : m_elements{ value, value, value, value }{
	}

	Element& x(){
		return m_elements[0];
	}
	const Element& x() const {
		return m_elements[0];
	}
	Element& y(){
		return m_elements[1];
	}
	const Element& y() const {
		return m_elements[1];
	}
	Element& z(){
		return m_elements[2];
	}
	const Element& z() const {
		return m_elements[2];
	}
	Element& w(){
		return m_elements[3];
	}
	const Element& w() const {
		return m_elements[3];
	}

	Element operator[]( std::size_t i ) const {
		return m_elements[i];
	}
	Element& operator[]( std::size_t i ){
		return m_elements[i];
	}

	Element* data(){
		return m_elements;
	}
	const Element* data() const {
		return m_elements;
	}

	BasicVector3<Element>& vec3(){
		return reinterpret_cast<BasicVector3<Element>&>( x() );
	}
	const BasicVector3<Element>& vec3() const {
		return reinterpret_cast<const BasicVector3<Element>&>( x() );
	}

	void set( const Element value ){
		x() = y() = z() = w() = value;
	}
};

template<typename Element>
inline BasicVector2<Element> vector2_from_array( const Element* array ){
	return BasicVector2<Element>( array[0], array[1] );
}

template<typename Element>
inline BasicVector3<Element> vector3_from_array( const Element* array ){
	return BasicVector3<Element>( array[0], array[1], array[2] );
}

template<typename Element>
inline Element* vector3_to_array( BasicVector3<Element>& self ){
	return self.data();
}
template<typename Element>
inline const Element* vector3_to_array( const BasicVector3<Element>& self ){
	return self.data();
}

template<typename Element>
inline Element* vector4_to_array( BasicVector4<Element>& self ){
	return self.data();
}
template<typename Element>
inline const Element* vector4_to_array( const BasicVector4<Element>& self ){
	return self.data();
}

template<typename Element>
inline BasicVector3<Element>& vector4_to_vector3( BasicVector4<Element>& self ){
	return *reinterpret_cast<BasicVector3<Element>*>( vector4_to_array( self ) );
}
template<typename Element>
inline const BasicVector3<Element>& vector4_to_vector3( const BasicVector4<Element>& self ){
	return *reinterpret_cast<const BasicVector3<Element>*>( vector4_to_array( self ) );
}

/// \brief A 2-element vector stored in single-precision floating-point.
typedef BasicVector2<float> Vector2;

/// \brief A 3-element vector stored in single-precision floating-point.
typedef BasicVector3<float> Vector3;

/// \brief A 3-element vector stored in double-precision floating-point.
typedef BasicVector3<double> DoubleVector3;

/// \brief A 4-element vector stored in single-precision floating-point.
typedef BasicVector4<float> Vector4;


template<typename TextOutputStreamType>
inline TextOutputStreamType& ostream_write( TextOutputStreamType& outputStream, const Vector3& v ){
	return outputStream << '(' << v.x() << ' ' << v.y() << ' ' << v.z() << ')';
}

template<typename TextOutputStreamType>
TextOutputStreamType& ostream_write( TextOutputStreamType& t, const Vector4& v ){
	return t << "[ " << v.x() << ' ' << v.y() << ' ' << v.z() << ' ' << v.w() << " ]";
}
