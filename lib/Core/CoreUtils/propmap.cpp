// kate: indent-width 4; auto-insert-doxygen on
//
// C++ Implementation: propmap
//
// Description:
//
//
// Author:  <Enrico Reimer>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "propmap.hpp"
#include <boost/foreach.hpp>
#include <boost/fusion/container/vector.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

namespace isis
{
namespace util
{
API_EXCLUDE_BEGIN;
/// @cond _internal
namespace _internal
{
/**
 * Continously searches in a sorted list using the given less-than comparison.
 * It starts at current and increments it until the referenced value is not less than the compare-value anymore.
 * Than it returns.
 * \param current the current-position-iterator for the sorted list.
 * This value is changed directly, so after the function returns is references the first entry of the list
 * which does not compare less than compare or, if such a value does not exit in the list, it will be equal to end.
 * \param end the end of the list
 * \param compare the compare-value
 * \param compOp the comparison functor. It must provide "bool operator()(T,T)".
 * \returns true if the value current currently refers to is equal to compare
 */
template<typename ForwardIterator, typename T, typename CMP> bool
continousFind( ForwardIterator &current, const ForwardIterator end, const T &compare, const CMP &compOp )
{
	//find the first iterator which does not compare less
	current = std::lower_bound( current, end, compare, compOp );

	if ( current == end //if we're at the end
		 || compOp( compare, *current ) //or compare less than that iterator
	   )
		return false;//we didn't find a match
	else
		return true;//not(current <> compare) makes compare == current
}
struct MapStrAdapter: boost::static_visitor<PropertyValue> {
	PropertyValue operator()( const PropertyValue &val )const {return val;}
	PropertyValue operator()( const PropertyMap &map )const {
		return PropertyValue( std::string( "[[PropertyMap with " ) + boost::lexical_cast<std::string>( map.container.size() ) + " entries]]" );
	}
};
struct RemoveEqualCheck: boost::static_visitor<bool> {
	bool removeNeeded;
	RemoveEqualCheck( bool _removeNeeded ): removeNeeded( removeNeeded ) {}
	bool operator()( PropertyValue &first, const PropertyValue &second )const { // if both are Values
		if( first != second ) {
			return false;
		} else // if they're not unequal (note empty values are neither equal nor unequal)
			return removeNeeded || !first.isNeeded();
	}
	bool operator()( PropertyMap &thisMap, const PropertyMap &otherMap )const { // recurse if both are subtree
		thisMap.removeEqual( otherMap );
		return false;
	}
	template<typename T1, typename T2> bool operator()( T1 &first, const T2 &second )const {return false;} // any other case
};
struct JoinTreeVisitor: boost::static_visitor<bool> {
	bool overwrite;
	PropertyMap::PathSet &rejects;
	const PropertyMap::PropPath &prefix, &name;
	JoinTreeVisitor( bool _overwrite, PropertyMap::PathSet &_rejects, const PropertyMap::PropPath &_prefix, const PropertyMap::PropPath &_name )
		: overwrite( _overwrite ), rejects( _rejects ), prefix( _prefix ), name( _name ) {}
	bool operator()( PropertyValue &first, const PropertyValue &second )const { // if both are Values
		if( first.isEmpty() || overwrite ) { // if ours is empty or overwrite is enabled
			first = second; //replace ours by the other
			return true;
		} else { // otherwise put the other into rejected if its not equal to our
			if( first != second )rejects.insert( rejects.end(), prefix / name );

			return false;
		}
	}
	bool operator()( PropertyMap &thisMap, const PropertyMap &otherMap )const { // recurse if both are subtree
		thisMap.joinTree( otherMap, overwrite, prefix / name, rejects );
		return false;
	}
	template<typename T1, typename T2> bool operator()( T1 &first, const T2 &second )const {return false;} // any other case
};
struct FlatMapMaker: boost::static_visitor<void> {
	PropertyMap::FlatMap &out;
	const PropertyMap::PropPath &name;
	FlatMapMaker( PropertyMap::FlatMap &_out, const PropertyMap::PropPath &_name ): out( _out ), name( _name ) {}
	void operator()( const PropertyValue &value )const {
		out[name] = value;
	}
	void operator()( const PropertyMap &map )const {
		for ( PropertyMap::container_type::const_iterator i = map.container.begin(); i != map.container.end(); i++ ) {
			boost::apply_visitor( FlatMapMaker( out, name / i->first ), i->second );
		}
	}
};
struct TreeInvalidCheck: boost::static_visitor<bool> {
	bool operator()( PropertyMap::container_type::const_reference ref ) const {
		return boost::apply_visitor( *this, ref.second );
	}//recursion
	bool operator()( const PropertyValue &val )const {
		return PropertyMap::invalidP()( val );
	}
	bool operator()( const PropertyMap &sub )const { //call my own recursion for each element
		return std::find_if( sub.container.begin(), sub.container.end(), *this ) != sub.container.end();
	}
};
}
/// @endcond _internal
API_EXCLUDE_END;

///////////////////////////////////////////////////////////////////
// PropPath impl
///////////////////////////////////////////////////////////////////

struct parser {
	typedef BOOST_TYPEOF( boost::spirit::ascii::space | '\t' | boost::spirit::eol ) skip_type;
	template<typename T> struct rule{typedef boost::spirit::qi::rule<uint8_t*, T(), skip_type > decl;};
	typedef boost::variant<PropertyValue, PropertyMap> value_cont;

	struct add_member {
		const char extra_token;
		add_member( char _extra_token ): extra_token( _extra_token ) {}
		void operator()( const boost::fusion::vector2<std::string, value_cont> &a, rule<PropertyMap>::decl::context_type &context, bool & )const {
			const PropertyMap::PropPath label = extra_token ?
			stringToList<PropertyMap::key_type>( a.m0, extra_token ) :
			PropertyMap::PropPath( a.m0.c_str() );
			PropertyMap &target = context.attributes.car;

			if( target.hasBranch( label ) || target.hasProperty( label ) )
				LOG( Runtime, error ) << "There is already an entry " << MSubject(target) << " skipping this one" ;
			else{
				const value_cont &container= a.m1;
				switch( container.which() ) {
				case 1:
					target.branch( label ) = boost::get<PropertyMap>( container );
					break;
				case 0:
					target.propertyValue( label ) = boost::get<PropertyValue>( container );
					break;
				}

			}
		}
	};

};

PropertyMap::PropPath::PropPath() {}
PropertyMap::PropPath::PropPath( const char *key ): std::list<key_type>( stringToList<key_type>( istring( key ), pathSeperator ) ) {}
PropertyMap::PropPath::PropPath( const key_type &key ): std::list<key_type>( stringToList<key_type>( key, pathSeperator ) ) {}
PropertyMap::PropPath::PropPath( const std::list<key_type> &path ): std::list<key_type>( path ) {}
PropertyMap::PropPath &PropertyMap::PropPath::operator/=( const PropertyMap::PropPath &s )
{
	insert( end(), s.begin(), s.end() );
	return *this;
}
PropertyMap::PropPath &PropertyMap::PropPath::operator/=( key_type s )
{
	push_back( s );
	return *this;
}
PropertyMap::PropPath PropertyMap::PropPath::operator/( const PropertyMap::PropPath &s )const {return PropPath( *this ) /= s;}
PropertyMap::PropPath PropertyMap::PropPath::operator/( key_type s )const {return PropPath( *this ) /= s;}

size_t PropertyMap::PropPath::length()const
{
	if( empty() )return 0;

	size_t ret = 0;
	BOOST_FOREACH( const_reference ref, *this )
	ret += ref.length();
	return ret + size() - 1;
}


///////////////////////////////////////////////////////////////////
// Contructors
///////////////////////////////////////////////////////////////////

PropertyMap::PropertyMap( const PropertyMap::container_type &src ): container( src ) {}
PropertyMap::PropertyMap() {}


///////////////////////////////////////////////////////////////////
// The core tree traversal functions
///////////////////////////////////////////////////////////////////
PropertyMap::mapped_type &PropertyMap::fetchEntry( const PropPath &path ) throw( boost::bad_get )
{
	return fetchEntry( container, path.begin(), path.end() );
}
PropertyMap::mapped_type &PropertyMap::fetchEntry( container_type &root, const propPathIterator at, const propPathIterator pathEnd ) throw( boost::bad_get )
{
	PropPath::const_iterator next = at;
	next++;
	container_type::iterator found = root.find( *at );

	if ( next != pathEnd ) {//we are not at the end of the path (a proposed leaf in the PropMap)
		if ( found != root.end() ) {//and we found the entry
			return fetchEntry( boost::get<PropertyMap>( found->second ).container, next, pathEnd ); //continue there
		} else { // if we should create a sub-map
			//insert a empty branch (aka PropMap) at "*at" (and fetch the reference of that)
			LOG( Debug, verbose_info ) << "Creating an empty branch " << *at << " trough fetching";
			return fetchEntry( boost::get<PropertyMap>( root[*at] = PropertyMap() ).container, next, pathEnd ); // and continue there (default value of the variant is PropertyValue, so init it to PropertyMap)
		}
	} else { //if its the leaf
		return root[*at]; // (create and) return that entry
	}
}

boost::optional<const PropertyMap::mapped_type &> PropertyMap::findEntry( const PropPath &path  )const throw( boost::bad_get )
{
	return findEntry( container, path.begin(), path.end() );
}

boost::optional<const PropertyMap::mapped_type &> PropertyMap::findEntry( const container_type &root, const propPathIterator at, const propPathIterator pathEnd )throw( boost::bad_get )
{
	propPathIterator next = at;
	next++;
	PropertyMap::container_type::const_iterator found = root.find( *at );

	if ( next != pathEnd ) {//we are not at the end of the path (aka the leaf)
		if ( found != root.end() ) {//and we found the entry
			return findEntry( boost::get<PropertyMap>( found->second ).container, next, pathEnd ); //continue there
		}
	} else if ( found != root.end() ) {// if its the leaf and we found the entry
		return found->second; // return that entry
	}

	return boost::optional<const PropertyMap::mapped_type &>();
}
bool PropertyMap::recursiveRemove( container_type &root, const propPathIterator pathIt, const propPathIterator pathEnd )throw( boost::bad_get )
{
	bool ret = false;

	if ( pathIt != pathEnd ) {
		propPathIterator next = pathIt;
		next++;
		const container_type::iterator found = root.find( *pathIt );

		if ( found != root.end() ) {
			if ( next != pathEnd ) {
				PropertyMap &ref = boost::get<PropertyMap>( found->second );
				ret = recursiveRemove( ref.container, next, pathEnd );

				if ( ref.isEmpty() )
					root.erase( found ); // remove the now empty branch
			} else {
				LOG_IF( found->second.type() == typeid( PropertyMap ) && !boost::get<PropertyMap>( found->second ).isEmpty(), Debug, warning )
						<< "Deleting non-empty branch " << MSubject( found->first );
				root.erase( found );
				ret = true;
			}
		} else {
			LOG( Runtime, warning ) << "Ignoring unknown entry " << *pathIt;
		}
	}

	return ret;
}


/////////////////////////////////////////////////////////////////////////////////////
// Interface for accessing elements
////////////////////////////////////////////////////////////////////////////////////

const PropertyValue &PropertyMap::propertyValue( const PropertyMap::PropPath &path )const
{
	return *tryFindEntry<PropertyValue>( path );
}

PropertyValue &PropertyMap::propertyValue( const PropertyMap::PropPath &path )
{
	return *tryFetchEntry<PropertyValue>( path );
}

const PropertyMap &PropertyMap::branch( const PropertyMap::PropPath &path ) const
{
	return *tryFindEntry<PropertyMap>( path );
}
PropertyMap &PropertyMap::branch( const PropPath &path )
{
	return *tryFetchEntry<PropertyMap>( path );
}

bool PropertyMap::remove( const PropPath &path )
{
	try {
		return recursiveRemove( container, path.begin(), path.end() );
	} catch( const boost::bad_get &e ) {
		LOG( Runtime, error ) << "Got errror " << e.what() << " when removing " << path << ", aborting the removal.";
		return false;
	}
}

bool PropertyMap::remove( const PathSet &removeList, bool keep_needed )
{
	bool ret = true;
	BOOST_FOREACH( PathSet::const_reference key, removeList ) {
		if( hasProperty( key ) ) { // remove everything which is there
			if( !( propertyValue( key ).isNeeded() && keep_needed ) ) { // if its not needed or keep_need is not true
				ret &= remove( key );
			}
		} else {
			LOG( Debug, notice ) << "Can't remove property " << key << " as its not there";
		}
	}
	return ret;
}


bool PropertyMap::remove( const PropertyMap &removeMap, bool keep_needed )
{
	container_type::iterator thisIt = container.begin();
	bool ret = true;

	//remove everything that is also in second
	for ( container_type::const_iterator otherIt = removeMap.container.begin(); otherIt != removeMap.container.end(); otherIt++ ) {
		//find the closest match for otherIt->first in this (use the value-comparison-functor of PropMap)
		if ( _internal::continousFind( thisIt, container.end(), *otherIt, container.value_comp() ) ) { //thisIt->first == otherIt->first - so its the same property or propmap
			if ( thisIt->second.type() == typeid( PropertyMap ) && otherIt->second.type() == typeid( PropertyMap ) ) { //both are a branch => recurse
				PropertyMap &mySub = boost::get<PropertyMap>( thisIt->second );
				const PropertyMap &otherSub = boost::get<PropertyMap>( otherIt->second );
				ret &= mySub.remove( otherSub );

				if( mySub.isEmpty() ) // delete my branch, if its empty
					container.erase( thisIt++ );
			} else if( thisIt->second.type() == typeid( PropertyValue ) && otherIt->second.type() == typeid( PropertyValue ) ) {
				container.erase( thisIt++ ); // so delete this (they are equal - kind of)
			} else { // this is a leaf
				LOG( Debug, warning ) << "Not deleting branch " << MSubject( thisIt->first ) << " because its no subtree on one side";
				ret = false;
			}
		}
	}

	return ret;
}


/////////////////////////////////////////////////////////////////////////////////////
// utilities
////////////////////////////////////////////////////////////////////////////////////
bool PropertyMap::isValid() const
{
	return !_internal::TreeInvalidCheck()( *this );
}

bool PropertyMap::isEmpty() const
{
	return container.empty();
}

PropertyMap::DiffMap PropertyMap::getDifference( const PropertyMap &other ) const
{
	PropertyMap::DiffMap ret;
	diffTree( other.container, ret, PropPath() );
	return ret;
}

void PropertyMap::diffTree( const container_type &other, PropertyMap::DiffMap &ret, const PropPath &prefix ) const
{
	container_type::const_iterator otherIt = other.begin();

	//insert everything that is in this, but not in second or is on both but differs
	for ( container_type::const_iterator thisIt = container.begin(); thisIt != container.end(); thisIt++ ) {
		//find the closest match for thisIt->first in other (use the value-comparison-functor of the container)
		if ( _internal::continousFind( otherIt, other.end(), *thisIt, container.value_comp() ) ) { //otherIt->first == thisIt->first - so its the same property
			if( thisIt->second.type() == typeid( PropertyMap ) && otherIt->second.type() == typeid( PropertyMap ) ) { // both are branches -- recursion step
				const PropertyMap &thisMap = boost::get<PropertyMap>( thisIt->second ), &refMap = boost::get<PropertyMap>( otherIt->second );
				thisMap.diffTree( refMap.container, ret, prefix / thisIt->first );
			} else if( thisIt->second.type() == typeid( PropertyValue ) && otherIt->second.type() == typeid( PropertyValue ) ) { // both are PropertyValue
				const PropertyValue &thisVal = boost::get<PropertyValue>( thisIt->second ), &refVal = boost::get<PropertyValue>( otherIt->second );

				if( thisVal != refVal ) // if they are different
					ret.insert( // add (propertyname|(value1|value2))
						ret.end(),      // we know it has to be at the end
						std::make_pair(
							prefix / thisIt->first,   //the key
							std::make_pair( thisVal, refVal ) //pair of both values
						)
					);
			} else { // obviously different just stuff it in
				ret.insert( ret.end(), std::make_pair( prefix / thisIt->first, std::make_pair(
						boost::apply_visitor( _internal::MapStrAdapter(), thisIt->second ),
						boost::apply_visitor( _internal::MapStrAdapter(), otherIt->second )
													   ) ) );
			}
		} else { // if ref is not in the other map
			const PropertyValue firstVal = boost::apply_visitor( _internal::MapStrAdapter(), thisIt->second );
			ret.insert( // add (propertyname|(value1|[empty]))
				ret.end(),      // we know it has to be at the end
				std::make_pair(
					prefix / thisIt->first,
					std::make_pair( firstVal, PropertyValue() )
				)
			);
		}
	}

	//insert everything that is in second but not in this
	container_type::const_iterator thisIt = container.begin();

	for ( otherIt = other.begin(); otherIt != other.end(); otherIt++ ) {
		if ( ! _internal::continousFind( thisIt, container.end(), *otherIt, container.value_comp() ) ) { //there is nothing in this which has the same key as ref

			const PropertyValue secondVal = boost::apply_visitor( _internal::MapStrAdapter(), otherIt->second );
			ret.insert(
				std::make_pair( // add (propertyname|([empty]|value2))
					prefix / otherIt->first,
					std::make_pair( PropertyValue(), secondVal )
				)
			);
		}
	}
}

void PropertyMap::removeEqual ( const PropertyMap &other, bool removeNeeded )
{
	container_type::iterator thisIt = container.begin();

	//remove everything that is also in second and equal (or also empty)
	for ( container_type::const_iterator otherIt = other.container.begin(); otherIt != other.container.end(); otherIt++ ) {
		//find the closest match for otherIt->first in this (use the value-comparison-functor of PropMap)
		if ( _internal::continousFind( thisIt, container.end(), *otherIt, container.value_comp() ) ) { //thisIt->first == otherIt->first  - so its the same property

			//          if(thisIt->second.type()==typeid(PropertyValue) && otherIt->second.type()==typeid(PropertyValue)){ // if both are Values
			if( boost::apply_visitor( _internal::RemoveEqualCheck( removeNeeded ), thisIt->second, otherIt->second ) ) {
				container.erase( thisIt++ ); // so delete this if both are empty _or_ equal
				continue; // keep iterator from incrementing again
			} else
				thisIt++;

			//              if(!(thisIt->second != otherIt->second)){ // and they're not unequal (note empty values are neither equal nor unequal)
			//                  if(removeNeeded || !boost::get<PropertyValue>(thisIt->second).isNeeded()){
			//                      LOG( Debug, verbose_info ) << "Removing " << *thisIt << " because its equal with the other (" << *otherIt << ")";
			//                      container.erase( thisIt++ ); // so delete this if bot are empty _or_ equal
			//                      continue; // keep iterator from incrementing again
			//                  } else
			//                      LOG( Debug, verbose_info ) << "Keeping " << *thisIt << " because it is needed";
			//              } else {
			//                  LOG( Debug, verbose_info ) << "Keeping " << *thisIt << " because it is not equal in the other (" << *otherIt << ")";
			//              }
			//          } else if(thisIt->second.type()==typeid(PropertyMap) && otherIt->second.type()==typeid(PropertyMap)){ // if both are subtree - recurse
			//              PropertyMap &thisMap = boost::get<PropertyMap>(thisIt->second);
			//              thisMap.removeEqual( boost::get<PropertyMap>(otherIt->second) );
			//          }
			//          thisIt++;
		}
	}
}


PropertyMap::PathSet PropertyMap::join( const PropertyMap &other, bool overwrite )
{
	PathSet rejects;
	joinTree( other, overwrite, PropPath(), rejects );
	return rejects;
}

void PropertyMap::joinTree( const PropertyMap &other, bool overwrite, const PropPath &prefix, PathSet &rejects )
{
	container_type::iterator thisIt = container.begin();

	for ( container_type::const_iterator otherIt = other.container.begin(); otherIt != other.container.end(); otherIt++ ) { //iterate through the elements of other
		if ( _internal::continousFind( thisIt, container.end(), *otherIt, container.value_comp() ) ) { // if the element is allready here
			if( boost::apply_visitor( _internal::JoinTreeVisitor( overwrite, rejects, prefix, thisIt->first ), thisIt->second, otherIt->second ) );

			LOG( Debug, verbose_info ) << "Replacing property " << MSubject( *thisIt ) << " by " << MSubject( otherIt->second );
		} else { // ok we dont have that - just insert it
			const std::pair<container_type::const_iterator, bool> inserted = container.insert( *otherIt );
			LOG_IF( inserted.second, Debug, verbose_info ) << "Inserted property " << MSubject( *inserted.first );
		}
	}
}

PropertyMap::FlatMap PropertyMap::getFlatMap() const
{
	FlatMap buff;
	_internal::FlatMapMaker( buff, PropPath() ).operator()( *this );
	return buff;
}


bool PropertyMap::transform( const PropPath &from,  const PropPath &to, int dstID, bool delSource )
{
	const PropertyValue &found = propertyValue( from );
	bool ret = false;

	if( ! found.isEmpty() ) {
		PropertyValue &dst = propertyValue( to );

		if ( found.getTypeID() == dstID ) {
			if( from != to ) {
				dst = found ;
				ret = true;
			} else {
				LOG( Debug, info ) << "Not transforming " << MSubject( found ) << " into same type at same place.";
			}
		} else {
			LOG_IF( from == to, Debug, notice ) << "Transforming " << MSubject( found ) << " in place.";
			PropertyValue buff = found.copyByID( dstID );

			if( buff.isEmpty() )
				ret = false;
			else {
				dst = buff;
				ret = true;
			}

			delSource &= ( from != to ); // dont remove the source, if its the destination as well
		}
	}

	if ( ret && delSource )remove( from );

	return ret;
}


PropertyMap::PathSet PropertyMap::getKeys()const   {return genKeyList<trueP>();}
PropertyMap::PathSet PropertyMap::getMissing()const {return genKeyList<invalidP>();}
PropertyMap::PathSet PropertyMap::getLists()const  {return genKeyList<listP>();}

void PropertyMap::addNeeded( const PropPath &path )
{
	propertyValue( path ).needed() = true;
}


boost::optional<const PropertyValue &> PropertyMap::hasProperty( const PropPath &path ) const
{
	boost::optional< const PropertyValue& > ref = tryFindEntry<PropertyValue>( path );

	if( ref && ! ref->isEmpty() ) {
		return ref;
	} else
		return boost::optional<const PropertyValue &>();
}

PropertyMap::PropPath PropertyMap::find( PropPath name, bool allowProperty, bool allowBranch ) const
{
	if( name.empty() ) {
		LOG( Debug, error ) << "Search key " << MSubject( name ) << " is invalid, won't search";
		return key_type();
	}

	LOG_IF( name.size() > 1, Debug, warning ) << "Stripping search key " << MSubject( name ) << " to " << name.back();

	// if the searched key is on this brach return its name
	container_type::const_iterator found = container.find( name.back() );

	if( found != container.end() &&
		( ( found->second.type() == typeid( PropertyValue ) && allowProperty ) || ( found->second.type() == typeid( PropertyMap ) && allowBranch ) )
	  ) {
		return found->first;
	} else { // otherwise search in the branches
		BOOST_FOREACH( container_type::const_reference ref, container ) {
			if( ref.second.type() == typeid( PropertyMap ) ) {
				const PropPath found = boost::get<PropertyMap>( ref.second ).find( name.back(), allowProperty, allowBranch );

				if( !found.empty() ) // if the key is found abort search and return it with its branch-name
					return PropPath( ref.first ) / found;
			}
		}
	}

	return PropPath(); // nothing found
}

boost::optional< const PropertyMap& > PropertyMap::hasBranch( const PropPath &path ) const
{
	return tryFindEntry<PropertyMap>( path );
}

bool PropertyMap::rename( const PropPath &oldname,  const PropPath &newname )
{
	if( oldname == newname ) // @todo makes pure syntactical rename "voxelSize => VoxelSize" impossible
		return false;

	boost::optional<const PropertyMap::mapped_type &> old_e = findEntry( oldname );

	if ( old_e ) {
		boost::optional<const PropertyMap::mapped_type &> new_e = findEntry( newname );
		LOG_IF( new_e && ! new_e->empty(), Runtime, warning ) << "Overwriting " << std::make_pair( newname, *new_e ) << " with " << *old_e;
		fetchEntry( newname ) = *old_e;
		return remove( oldname );
	} else {
		LOG( Runtime, warning ) << "Cannot rename " << oldname << " it does not exist";
		return false;
	}
}

void PropertyMap::toCommonUnique( PropertyMap &common, PathSet &uniques )const
{
	const DiffMap difference = common.getDifference( *this );
	BOOST_FOREACH( const DiffMap::value_type & ref, difference ) {
		uniques.insert( ref.first );

		if ( ! ref.second.first.isEmpty() ) {
			LOG( Debug, verbose_info ) << "Detected difference in " << ref << " removing from common";
			common.remove( ref.first );//if there is something in common, remove it
		}
	}
}

bool PropertyMap::readJson( uint8_t* streamBegin, uint8_t* streamEnd, char extra_token )
{
	using namespace boost::spirit;
	using qi::lit;

	parser::rule<boost::fusion::vector2<std::string, parser::value_cont> >::decl member;
	
	parser::rule<std::string>::decl string( lexeme['"' >> *( ascii::print - '"' ) >> '"'], "string" );
	parser::rule<std::string>::decl label( string >> ':', "label" );
	parser::rule<int>::decl integer( int_ >> !lit( '.' ), "integer" ) ; // an integer followed by a '.' is not an integer
	parser::rule<dlist>::decl dlist( lit( '[' ) >> double_ % ',' >> ']', "dlist" );
	parser::rule<ilist>::decl ilist( lit( '[' ) >> integer % ',' >> ']', "ilist" );
	parser::rule<slist>::decl slist( lit( '[' ) >> string  % ',' >> ']', "slist" );
	parser::rule<PropertyValue>::decl value = integer | double_ | string | ilist | dlist | slist;
	parser::rule<PropertyValue>::decl vallist( lit( '[' ) >> value % ',' >> ']', "value_list" );

	parser::rule<PropertyMap>::decl object( lit( '{' ) >> ( member[parser::add_member( extra_token )] % ',' || eps ) >> '}', "object" );
	
	member = label >> ( value | vallist | object );
	
	uint8_t* end = streamEnd;
	bool erg = qi::phrase_parse( streamBegin, end, object[boost::phoenix::ref( *this ) = _1], ascii::space | '\t' | eol );
	return end == streamEnd;
}


std::ostream &PropertyMap::print( std::ostream &out, bool label )const
{
	FlatMap buff = getFlatMap();
	size_t key_len = 0;

	for ( FlatMap::const_iterator i = buff.begin(); i != buff.end(); i++ )
		if ( key_len < i->first.length() )
			key_len = i->first.length();

	for ( FlatMap::const_iterator i = buff.begin(); i != buff.end(); i++ )
		out << i->first << std::string( key_len - i->first.length(), ' ' ) + ":" << i->second.toString( label ) << std::endl;

	return out;
}

bool PropertyMap::listP::operator()( const PropertyValue &ref ) const {return ref.size() > 1;}
bool PropertyMap::trueP::operator()( const PropertyValue &/*ref*/ ) const {return true;}
bool PropertyMap::invalidP::operator()( const PropertyValue &ref ) const {return ref.isNeeded() && ref.isEmpty();}

}
}
