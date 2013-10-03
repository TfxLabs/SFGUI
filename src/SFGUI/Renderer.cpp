#include <SFGUI/Renderer.hpp>
#include <SFGUI/Renderers/VertexArrayRenderer.hpp>
#include <SFGUI/Renderers/VertexBufferRenderer.hpp>
#include <SFGUI/Context.hpp>
#include <SFGUI/RendererViewport.hpp>

#include <cmath>
#include <cstring>

namespace sfg {

SharedPtr<Renderer> Renderer::m_instance = SharedPtr<Renderer>();

Renderer::Renderer() :
	m_vertex_count( 0 ),
	m_index_count( 0 ),
	m_window_size( 0, 0 ),
	m_max_texture_size( 0 ),
	m_force_redraw( false ),
	m_last_window_size( 0, 0 ) {
	// Needed to determine maximum texture size.
	sf::Context context;

	m_default_viewport = CreateViewport();
	m_max_texture_size = sf::Texture::getMaximumSize();

	// Load our "no texture" pseudo-texture.
	sf::Image pseudo_image;
	pseudo_image.create( 2, 2, sf::Color::White );
	m_pseudo_texture = LoadTexture( pseudo_image );
}

Renderer::~Renderer() {
	while( !m_texture_atlas.empty() ) {
		delete m_texture_atlas.back();
		m_texture_atlas.pop_back();
	}
}

Renderer& Renderer::Create() {
	if( !m_instance ) {
		if( VertexBufferRenderer::IsAvailable() ) {
			m_instance.reset( new VertexBufferRenderer );
		}
		else {
			m_instance.reset( new VertexArrayRenderer );
		}
	}

	return *m_instance;
}

Renderer& Renderer::Get() {
	if( !m_instance ) {
#ifdef SFGUI_DEBUG
		std::cerr << "Renderer not created yet. Did you create an sfg::SFGUI object?\n";
#endif
		Create();
	}

	return *m_instance;
}

void Renderer::Set( const SharedPtr<Renderer>& renderer ) {
	if( renderer ) {
		m_instance = renderer;
	}
}

void Renderer::Destroy() {
	m_instance.reset();
}

bool Renderer::Exists() {
	return m_instance;
}

const RendererViewport::Ptr& Renderer::GetDefaultViewport() {
	return m_default_viewport;
}

RendererViewport::Ptr Renderer::CreateViewport() {
	RendererViewport::Ptr viewport( new RendererViewport );

	return viewport;
}

Primitive::Ptr Renderer::CreateText( const sf::Text& text ) {
	const sf::Font& font = *text.getFont();
	unsigned int character_size = text.getCharacterSize();
	sf::Color color = text.getColor();

	sf::Vector2f atlas_offset = LoadFont( font, character_size );

	const sf::String& str = text.getString();
	std::size_t length = str.getSize();

	float horizontal_spacing = static_cast<float>( font.getGlyph( L' ', character_size, false ).advance );
	float vertical_spacing = static_cast<float>( Context::Get().GetEngine().GetFontLineHeight( font, character_size ) );
	sf::Vector2f start_position( std::floor( text.getPosition().x + .5f ), std::floor( text.getPosition().y + static_cast<float>( character_size ) + .5f ) );

	sf::Vector2f position( start_position );

	const static float tab_spaces = 2.f;

	sf::Uint32 previous_character = 0;

	Primitive::Ptr primitive( new Primitive( length * 4 ) );

	Primitive character_primitive( 4 );

	for( std::size_t index = 0; index < length; ++index ) {
		sf::Uint32 current_character = str[index];

		position.x += static_cast<float>( font.getKerning( previous_character, current_character, character_size ) );

		switch( current_character ) {
			case L' ':
				position.x += horizontal_spacing;
				continue;
			case L'\t':
				position.x += horizontal_spacing * tab_spaces;
				continue;
			case L'\n':
				position.y += vertical_spacing;
				position.x = start_position.x;
				continue;
			case L'\v':
				position.y += vertical_spacing * tab_spaces;
				continue;
			default:
				break;
		}

		const sf::Glyph& glyph = font.getGlyph( current_character, character_size, false );

		Primitive::Vertex vertex0;
		Primitive::Vertex vertex1;
		Primitive::Vertex vertex2;
		Primitive::Vertex vertex3;

		vertex0.position = position + sf::Vector2f( static_cast<float>( glyph.bounds.left ), static_cast<float>( glyph.bounds.top ) );
		vertex1.position = position + sf::Vector2f( static_cast<float>( glyph.bounds.left ), static_cast<float>( glyph.bounds.top + glyph.bounds.height ) );
		vertex2.position = position + sf::Vector2f( static_cast<float>( glyph.bounds.left + glyph.bounds.width ), static_cast<float>( glyph.bounds.top ) );
		vertex3.position = position + sf::Vector2f( static_cast<float>( glyph.bounds.left + glyph.bounds.width ), static_cast<float>( glyph.bounds.top + glyph.bounds.height ) );

		vertex0.color = color;
		vertex1.color = color;
		vertex2.color = color;
		vertex3.color = color;

		// Let SFML cast the Rect for us.
		sf::FloatRect texture_rect( glyph.textureRect );

		vertex0.texture_coordinate = atlas_offset + sf::Vector2f( texture_rect.left, texture_rect.top );
		vertex1.texture_coordinate = atlas_offset + sf::Vector2f( texture_rect.left, texture_rect.top + texture_rect.height );
		vertex2.texture_coordinate = atlas_offset + sf::Vector2f( texture_rect.left + texture_rect.width, texture_rect.top );
		vertex3.texture_coordinate = atlas_offset + sf::Vector2f( texture_rect.left + texture_rect.width, texture_rect.top + texture_rect.height );

		character_primitive.Clear();

		character_primitive.AddVertex( vertex0 );
		character_primitive.AddVertex( vertex1 );
		character_primitive.AddVertex( vertex2 );
		character_primitive.AddVertex( vertex2 );
		character_primitive.AddVertex( vertex1 );
		character_primitive.AddVertex( vertex3 );

		primitive->Add( character_primitive );

		position.x += static_cast<float>( glyph.advance );

		previous_character = current_character;
	}

	AddPrimitive( primitive );

	return primitive;
}

Primitive::Ptr Renderer::CreateQuad( const sf::Vector2f& top_left, const sf::Vector2f& bottom_left,
                                     const sf::Vector2f& bottom_right, const sf::Vector2f& top_right,
                                     const sf::Color& color ) {
	Primitive::Ptr primitive( new Primitive( 4 ) );

	Primitive::Vertex vertex0;
	Primitive::Vertex vertex1;
	Primitive::Vertex vertex2;
	Primitive::Vertex vertex3;

	vertex0.position = sf::Vector2f( std::floor( top_left.x + .5f ), std::floor( top_left.y + .5f ) );
	vertex1.position = sf::Vector2f( std::floor( bottom_left.x + .5f ), std::floor( bottom_left.y + .5f ) );
	vertex2.position = sf::Vector2f( std::floor( top_right.x + .5f ), std::floor( top_right.y + .5f ) );
	vertex3.position = sf::Vector2f( std::floor( bottom_right.x + .5f ), std::floor( bottom_right.y + .5f ) );

	vertex0.color = color;
	vertex1.color = color;
	vertex2.color = color;
	vertex3.color = color;

	vertex0.texture_coordinate = sf::Vector2f( 0.f, 0.f );
	vertex1.texture_coordinate = sf::Vector2f( 0.f, 1.f );
	vertex2.texture_coordinate = sf::Vector2f( 1.f, 0.f );
	vertex3.texture_coordinate = sf::Vector2f( 1.f, 1.f );

	primitive->AddVertex( vertex0 );
	primitive->AddVertex( vertex1 );
	primitive->AddVertex( vertex2 );
	primitive->AddVertex( vertex2 );
	primitive->AddVertex( vertex1 );
	primitive->AddVertex( vertex3 );

	AddPrimitive( primitive );

	return primitive;
}

Primitive::Ptr Renderer::CreatePane( const sf::Vector2f& position, const sf::Vector2f& size, float border_width,
                                     const sf::Color& color, const sf::Color& border_color, int border_color_shift ) {
	if( border_width <= 0.f ) {
		return CreateRect( position, position + size, color );
	}

	Primitive::Ptr primitive( new Primitive( 20 ) );

	sf::Color dark_border( border_color );
	sf::Color light_border( border_color );

	Context::Get().GetEngine().ShiftBorderColors( light_border, dark_border, border_color_shift );

	float left = position.x;
	float top = position.y;
	float right = left + size.x;
	float bottom = top + size.y;

	Primitive::Ptr rect(
		CreateQuad(
			sf::Vector2f( left + border_width, top + border_width ),
			sf::Vector2f( left + border_width, bottom - border_width ),
			sf::Vector2f( right - border_width, bottom - border_width ),
			sf::Vector2f( right - border_width, top + border_width ),
			color
		)
	);

	Primitive::Ptr line_top(
		CreateLine(
			sf::Vector2f( left + border_width / 2.f, top + border_width / 2.f ),
			sf::Vector2f( right - border_width / 2.f, top + border_width / 2.f ),
			light_border,
			border_width
		)
	);

	Primitive::Ptr line_right(
		CreateLine(
			sf::Vector2f( right - border_width / 2.f, top + border_width / 2.f ),
			sf::Vector2f( right - border_width / 2.f, bottom - border_width / 2.f ),
			dark_border,
			border_width
		)
	);

	Primitive::Ptr line_bottom(
		CreateLine(
			sf::Vector2f( right - border_width / 2.f, bottom - border_width / 2.f ),
			sf::Vector2f( left + border_width / 2.f, bottom - border_width / 2.f ),
			dark_border,
			border_width
		)
	);

	Primitive::Ptr line_left(
		CreateLine(
			sf::Vector2f( left + border_width / 2.f, bottom - border_width / 2.f ),
			sf::Vector2f( left + border_width / 2.f, top + border_width / 2.f ),
			light_border,
			border_width
		)
	);

	primitive->Add( *rect.get() );
	primitive->Add( *line_top.get() );
	primitive->Add( *line_right.get() );
	primitive->Add( *line_bottom.get() );
	primitive->Add( *line_left.get() );

	std::vector<Primitive::Ptr>::iterator iter( std::find( m_primitives.begin(), m_primitives.end(), rect ) );

	assert( iter != m_primitives.end() );

	iter = m_primitives.erase( iter ); // rect
	iter = m_primitives.erase( iter ); // line_top
	iter = m_primitives.erase( iter ); // line_right
	iter = m_primitives.erase( iter ); // line_bottom
	m_primitives.erase( iter ); // line_left

	AddPrimitive( primitive );

	return primitive;
}

Primitive::Ptr Renderer::CreateRect( const sf::Vector2f& top_left, const sf::Vector2f& bottom_right, const sf::Color& color ) {
	return CreateQuad(
		sf::Vector2f( top_left.x, top_left.y ),
		sf::Vector2f( top_left.x, bottom_right.y ),
		sf::Vector2f( bottom_right.x, bottom_right.y ),
		sf::Vector2f( bottom_right.x, top_left.y ),
		color
	);
}

Primitive::Ptr Renderer::CreateRect( const sf::FloatRect& rect, const sf::Color& color ) {
	return CreateRect( sf::Vector2f( rect.left, rect.top ), sf::Vector2f( rect.left + rect.width, rect.top + rect.height ), color );
}

Primitive::Ptr Renderer::CreateTriangle( const sf::Vector2f& point0, const sf::Vector2f& point1, const sf::Vector2f& point2, const sf::Color& color ) {
	Primitive::Ptr primitive( new Primitive( 3 ) );

	Primitive::Vertex vertex0;
	Primitive::Vertex vertex1;
	Primitive::Vertex vertex2;

	vertex0.position = point0;
	vertex1.position = point1;
	vertex2.position = point2;

	vertex0.color = color;
	vertex1.color = color;
	vertex2.color = color;

	vertex0.texture_coordinate = sf::Vector2f( 0.f, 0.f );
	vertex1.texture_coordinate = sf::Vector2f( 0.f, 1.f );
	vertex2.texture_coordinate = sf::Vector2f( 1.f, 0.f );

	primitive->AddVertex( vertex0 );
	primitive->AddVertex( vertex1 );
	primitive->AddVertex( vertex2 );

	AddPrimitive( primitive );

	return primitive;
}

Primitive::Ptr Renderer::CreateSprite( const sf::FloatRect& rect, const Primitive::Texture::Ptr& texture, const sf::FloatRect& subrect, int rotation_turns ) {
	sf::Vector2f offset = texture->offset;

	Primitive::Ptr primitive( new Primitive( 4 ) );

	Primitive::Vertex vertex0;
	Primitive::Vertex vertex1;
	Primitive::Vertex vertex2;
	Primitive::Vertex vertex3;

	vertex0.position = sf::Vector2f( std::floor( rect.left + .5f ), std::floor( rect.top + .5f ) );
	vertex1.position = sf::Vector2f( std::floor( rect.left + .5f ), std::floor( rect.top + .5f ) + std::floor( rect.height + .5f ) );
	vertex2.position = sf::Vector2f( std::floor( rect.left + .5f ) + std::floor( rect.width + .5f ), std::floor( rect.top + .5f ) );
	vertex3.position = sf::Vector2f( std::floor( rect.left + .5f ) + std::floor( rect.width + .5f ), std::floor( rect.top + .5f ) + std::floor( rect.height + .5f ) );

	vertex0.color = sf::Color( 255, 255, 255, 255 );
	vertex1.color = sf::Color( 255, 255, 255, 255 );
	vertex2.color = sf::Color( 255, 255, 255, 255 );
	vertex3.color = sf::Color( 255, 255, 255, 255 );

	sf::Vector2f coords[4];

	if( ( subrect.left != 0.f ) || ( subrect.top != 0.f ) || ( subrect.width != 0.f ) || ( subrect.height != 0.f ) ) {
		coords[0] = offset + sf::Vector2f( std::floor( subrect.left + .5f ), std::floor( subrect.top + .5f ) );
		coords[3] = offset + sf::Vector2f( std::floor( subrect.left + .5f ), std::floor( subrect.top + .5f ) ) + sf::Vector2f( 0.f, std::floor( subrect.height + .5f ) );
		coords[1] = offset + sf::Vector2f( std::floor( subrect.left + .5f ), std::floor( subrect.top + .5f ) ) + sf::Vector2f( std::floor( subrect.width + .5f ), 0.f );
		coords[2] = offset + sf::Vector2f( std::floor( subrect.left + .5f ), std::floor( subrect.top + .5f ) ) + sf::Vector2f( std::floor( subrect.width + .5f ), std::floor( subrect.height + .5f ) );
	}
	else {
		coords[0] = offset + sf::Vector2f( 0.f, 0.f );
		coords[3] = offset + sf::Vector2f( 0.f, static_cast<float>( texture->size.y ) );
		coords[1] = offset + sf::Vector2f( static_cast<float>( texture->size.x ), 0.f );
		coords[2] = offset + sf::Vector2f( static_cast<float>( texture->size.x ), static_cast<float>( texture->size.y ) );
	}

	// Get rotation_turns into the range [0;3].
	for( ; rotation_turns < 0; rotation_turns += 4 );
	for( ; rotation_turns > 3; rotation_turns -= 4 );

	// Perform the circular shift.
	if( rotation_turns != 0 ) {
		std::rotate( coords, coords + rotation_turns, coords + 4 );
	}

	vertex0.texture_coordinate = coords[0];
	vertex1.texture_coordinate = coords[3];
	vertex2.texture_coordinate = coords[1];
	vertex3.texture_coordinate = coords[2];

	primitive->AddVertex( vertex0 );
	primitive->AddVertex( vertex1 );
	primitive->AddVertex( vertex2 );
	primitive->AddVertex( vertex2 );
	primitive->AddVertex( vertex1 );
	primitive->AddVertex( vertex3 );

	primitive->AddTexture( texture );

	AddPrimitive( primitive );

	return primitive;
}

Primitive::Ptr Renderer::CreateLine( const sf::Vector2f& begin, const sf::Vector2f& end, const sf::Color& color, float thickness ) {
	// Get vector perpendicular to direction of the line vector.
	// Vector is rotated CCW 90 degrees and normalized.
	sf::Vector2f normal( end - begin );
	sf::Vector2f unrotated_normal( normal );
	std::swap( normal.x, normal.y );
	float length = std::sqrt( normal.x * normal.x + normal.y * normal.y );
	normal.x /= -length;
	normal.y /= length;
	unrotated_normal.x /= length;
	unrotated_normal.y /= length;

	sf::Vector2f corner0( begin + normal * ( thickness * .5f ) - unrotated_normal * ( thickness * .5f ) );
	sf::Vector2f corner1( begin - normal * ( thickness * .5f ) - unrotated_normal * ( thickness * .5f ) );
	sf::Vector2f corner2( end - normal * ( thickness * .5f ) + unrotated_normal * ( thickness * .5f ) );
	sf::Vector2f corner3( end + normal * ( thickness * .5f ) + unrotated_normal * ( thickness * .5f ) );

	return CreateQuad( corner3, corner2, corner1, corner0, color );
}

Primitive::Ptr Renderer::CreateGLCanvas( SharedPtr<Signal> callback ) {
	Primitive::Ptr primitive( new Primitive );

	primitive->SetCustomDrawCallback( callback );

	AddPrimitive( primitive );

	return primitive;
}

void Renderer::Display( sf::Window& target ) const {
	m_window_size = target.getSize();

	target.setActive( true );

	glPushClientAttrib( GL_CLIENT_VERTEX_ARRAY_BIT );
	glPushAttrib( GL_COLOR_BUFFER_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT );

	// Since we have no idea what the attribute environment
	// of the user looks like, we need to pretend to be SFML
	// by setting up it's GL attribute environment.
	glEnable( GL_TEXTURE_2D );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_COLOR_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	SetupGL();

	DisplayImpl();

	RestoreGL();

	glPopAttrib();
	glPopClientAttrib();
}

void Renderer::Display( sf::RenderWindow& target ) const {
	m_window_size = target.getSize();

	target.setActive( true );

	SetupGL();

	DisplayImpl();

	RestoreGL();

	WipeStateCache( target );
}

void Renderer::Display( sf::RenderTexture& target ) const {
	m_window_size = target.getSize();

	target.setActive( true );

	SetupGL();

	DisplayImpl();

	RestoreGL();

	WipeStateCache( target );
}

void Renderer::SetupGL() const {
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();

	// When SFML dies (closes) it sets the window size to 0 for some reason.
	// That then causes glOrtho errors.

	// SFML doesn't seem to bother updating the OpenGL viewport when
	// it's window resizes and nothing is drawn directly through SFML...

	if( m_last_window_size != m_window_size ) {
		glViewport( 0, 0, static_cast<GLsizei>( m_window_size.x ), static_cast<GLsizei>( m_window_size.y ) );

		m_last_window_size = m_window_size;

		if( m_window_size.x && m_window_size.y ) {
			const_cast<Renderer*>( this )->Invalidate( INVALIDATE_VERTEX | INVALIDATE_TEXTURE );

			const_cast<Renderer*>( this )->InvalidateWindow();
		}
	}

	glOrtho( 0.0f, static_cast<GLdouble>( m_window_size.x ? m_window_size.x : 1 ), static_cast<GLdouble>( m_window_size.y ? m_window_size.y : 1 ), 0.0f, -1.0f, 64.0f );

	glMatrixMode( GL_TEXTURE );
	glPushMatrix();
	glLoadIdentity();

	glEnable( GL_CULL_FACE );
}

void Renderer::RestoreGL() const {
	glDisable( GL_CULL_FACE );

	glPopMatrix();

	glMatrixMode( GL_PROJECTION );
	glPopMatrix();

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();
}

void Renderer::WipeStateCache( sf::RenderTarget& target ) const {
	// Make SFML disable it's **************** vertex cache without us
	// having to call ResetGLStates() and disturbing OpenGL needlessly.
	// This would be sooo much easier if we could somehow set
	// myCache.UseVertexCache = false;
	// in window by ourself every frame.
	// SFML doesn't like to play nice with other VBO / Vertex Array
	// users, that's for sure... It assumes that the array pointers
	// don't get changed between calls to Draw() unless ResetGLStates()
	// is explicitly called in between. Why do we need to call OpenGL
	// 10+ times just to reset something this simple? No logic.

	//static sf::VertexArray resetter_array( sf::TrianglesStrip, 5 );
	//window.Draw( resetter_array );

	// Or... even more evil... use memset to blank the StatesCache of
	// the RenderWindow with zeros. Thankfully, because we are using
	// the data structures directly from the SFML headers, if Laurent
	// decides to change their size one day we won't even notice.
	struct StatesCache {
		bool glStatesSet;
		bool ViewChanged;
		sf::BlendMode LastBlendMode;
		sf::Uint64 LastTextureId;
		bool UseVertexCache;
		sf::Vertex VertexCache[4];
	};

	// All your cache are belong to us.
	memset( reinterpret_cast<char*>( &target ) + sizeof( sf::RenderTarget ) - sizeof( StatesCache ) + 1, 0, sizeof( StatesCache ) - 1 );

	// This is to make it forget it's cached viewport.
	// Seriously... caching the viewport? Come on...
	memset( reinterpret_cast<char*>( &target ) + sizeof( sf::RenderTarget ) - sizeof( StatesCache ) + 1, 1, 1 );

	// Since we wiped SFML's cache of its bound texture, we
	// should make sure that it stays coherent with reality :).
	sf::Texture::bind( 0 );
}

sf::Vector2f Renderer::LoadFont( const sf::Font& font, unsigned int size ) {
	// Get the font face that Laurent tries to hide from us.
	struct FontStruct {
		void* font_face; // Authentic SFML comment: implementation details
		void* unused1;
		int* unused2;

		// Since maps allocate everything non-contiguously on the heap we can use void* instead of Page here.
		mutable std::map<unsigned int, void*> unused3;
		mutable std::vector<sf::Uint8> unused4;
	};

	void* face;

	// All your font face are belong to us too.
	memcpy( &face, reinterpret_cast<const char*>( &font ) + sizeof( sf::Font ) - sizeof( FontStruct ), sizeof( void* ) );

	FontID id( face, size );

	std::map<FontID, Primitive::Texture::Ptr >::iterator iter( m_fonts.find( id ) );

	if( iter != m_fonts.end() ) {
		return iter->second->offset;
	}

	// Make sure all the glyphs we need are loaded.
	for( sf::Uint32 codepoint = 0; codepoint < 0x0370; ++codepoint ) {
		font.getGlyph( codepoint, size, false );
	}

	sf::Image image = font.getTexture( size ).copyToImage();

	Primitive::Texture::Ptr handle = LoadTexture( image );

	m_fonts[id] = handle;

	return handle->offset;
}

Primitive::Texture::Ptr Renderer::LoadTexture( const sf::Texture& texture ) {
	return LoadTexture( texture.copyToImage() );
}

Primitive::Texture::Ptr Renderer::LoadTexture( const sf::Image& image ) {
	if( ( image.getSize().x > m_max_texture_size ) || ( image.getSize().x > m_max_texture_size ) ) {
#ifdef SFGUI_DEBUG
		std::cerr << "SFGUI warning: The image you are using is larger than the maximum size supported by your GPU (" << m_max_texture_size << "x" << m_max_texture_size << ").\n";
#endif
		return Primitive::Texture::Ptr( new Primitive::Texture );
	}

	// We insert padding between atlas elements to prevent
	// texture filtering from screwing up our images.
	// If 1 pixel isn't enough, increase.
	const static unsigned int padding = 1;

	// Look for a nice insertion point for our new texture.
	// We use first fit and according to theory it is never
	// worse than double the optimum size.
	std::list<TextureNode>::iterator iter = m_textures.begin();

	float last_occupied_location = 0.f;

	for( ; iter != m_textures.end(); ++iter ) {
		float space_available = iter->offset.y - last_occupied_location;

		if( space_available >= static_cast<float>( image.getSize().y + 2 * padding ) ) {
			// We found a nice spot.
			break;
		}

		last_occupied_location = iter->offset.y + static_cast<float>( iter->size.y );
	}

	std::size_t current_page = static_cast<unsigned int>( last_occupied_location ) / m_max_texture_size;
	last_occupied_location = static_cast<float>( static_cast<unsigned int>( last_occupied_location ) % m_max_texture_size );

	if( m_texture_atlas.empty() || ( ( static_cast<unsigned int>( last_occupied_location ) % m_max_texture_size ) + image.getSize().y + 2 * padding > m_max_texture_size ) ) {
		// We need a new atlas page.
		m_texture_atlas.push_back( new sf::Texture() );

		current_page = m_texture_atlas.size() - 1;

		last_occupied_location = 0.f;
	}

	if( ( image.getSize().x > m_texture_atlas[current_page]->getSize().x ) || ( last_occupied_location + static_cast<float>( image.getSize().y ) > static_cast<float>( m_texture_atlas[current_page]->getSize().y ) ) ) {
		// Image is loaded into atlas after expanding texture atlas.
		sf::Image old_image = m_texture_atlas[current_page]->copyToImage();
		sf::Image new_image;

		new_image.create( std::max( old_image.getSize().x, image.getSize().x ), static_cast<unsigned int>( std::floor( last_occupied_location + .5f ) ) + image.getSize().y + padding, sf::Color::White );
		new_image.copy( old_image, 0, 0 );

		new_image.copy( image, 0, static_cast<unsigned int>( std::floor( last_occupied_location + .5f ) ) + padding );

		m_texture_atlas[current_page]->loadFromImage( new_image );
	}
	else {
		// Image is loaded into atlas.
		sf::Image atlas_image = m_texture_atlas[current_page]->copyToImage();

		atlas_image.copy( image, 0, static_cast<unsigned int>( std::floor( last_occupied_location + .5f ) ) + padding );

		m_texture_atlas[current_page]->loadFromImage( atlas_image );
	}

	sf::Vector2f offset = sf::Vector2f( 0.f, static_cast<float>( current_page * m_max_texture_size ) + last_occupied_location + static_cast<float>( padding ) );

	Invalidate( INVALIDATE_TEXTURE );

	Primitive::Texture::Ptr handle( new Primitive::Texture );

	handle->offset = offset;
	handle->size = image.getSize();

	TextureNode texture_node;
	texture_node.offset = offset;
	texture_node.size = image.getSize();

	m_textures.insert( iter, texture_node );

	return handle;
}

void Renderer::UnloadImage( const sf::Vector2f& offset ) {
	for( std::list<TextureNode>::iterator iter = m_textures.begin(); iter != m_textures.end(); ++iter ) {
		if( iter->offset == offset ) {
			m_textures.erase( iter );
			return;
		}
	}

// Only enable during development.
//#ifdef SFGUI_DEBUG
//	std::cerr << "Tried to unload non-existant image at (" << offset.x << "," << offset.y << ").\n";
//#endif
}

void Renderer::UpdateImage( const sf::Vector2f& offset, const sf::Image& data ) {
	for( std::list<TextureNode>::iterator iter = m_textures.begin(); iter != m_textures.end(); ++iter ) {
		if( iter->offset == offset ) {
			if( iter->size != data.getSize() ) {
#ifdef SFGUI_DEBUG
				std::cerr << "Tried to update texture with mismatching image size.\n";
#endif
				return;
			}

			std::size_t page = static_cast<std::size_t>( offset.y ) / m_max_texture_size;

			sf::Image image = m_texture_atlas[page]->copyToImage();
			image.copy( data, 0, static_cast<unsigned int>( std::floor( offset.y + .5f ) ) % m_max_texture_size );
			m_texture_atlas[page]->loadFromImage( image );

			return;
		}
	}

// Only enable during development.
//#ifdef SFGUI_DEBUG
//	std::cerr << "Tried to update non-existant image at (" << offset.x << "," << offset.y << ").\n";
//#endif
}

void Renderer::SortPrimitives() {
	std::size_t current_position = 1;
	std::size_t sort_index;

	std::size_t primitives_size = m_primitives.size();

	// Classic insertion sort.
	while( current_position < primitives_size ) {
		sort_index = current_position++;

		while( ( sort_index > 0 ) && ( m_primitives[sort_index - 1]->GetLayer() * 1048576 + m_primitives[sort_index - 1]->GetLevel() > m_primitives[sort_index]->GetLayer() * 1048576 + m_primitives[sort_index]->GetLevel() ) ) {
			m_primitives[sort_index].swap( m_primitives[sort_index - 1] );
			--sort_index;
		}
	}
}

void Renderer::AddPrimitive( const Primitive::Ptr& primitive ) {
	m_primitives.push_back( primitive );

	// Check for alpha values in primitive.
	// Disable depth test if any found.
	const std::vector<Primitive::Vertex>& vertices( primitive->GetVertices() );
	const std::vector<GLuint>& indices( primitive->GetIndices() );

	std::size_t vertices_size = vertices.size();
	std::size_t indices_size = indices.size();

	m_vertex_count += vertices_size;
	m_index_count += indices_size;

	Invalidate( INVALIDATE_ALL );
}

void Renderer::RemovePrimitive( const Primitive::Ptr& primitive ) {
	std::vector<Primitive::Ptr>::iterator iter( std::find( m_primitives.begin(), m_primitives.end(), primitive ) );

	if( iter != m_primitives.end() ) {
		const std::vector<Primitive::Vertex>& vertices( (*iter)->GetVertices() );
		const std::vector<GLuint>& indices( (*iter)->GetIndices() );

		assert( m_vertex_count >= vertices.size() );
		assert( m_index_count >= indices.size() );

		m_vertex_count -= vertices.size();
		m_index_count -= indices.size();

		m_primitives.erase( iter );
	}

	Invalidate( INVALIDATE_ALL );
}

void Renderer::Invalidate( unsigned char datasets ) {
	InvalidateImpl( datasets );
}

void Renderer::Redraw() {
	m_force_redraw = true;
}

const sf::Vector2u& Renderer::GetWindowSize() const {
	return m_last_window_size;
}

void Renderer::InvalidateImpl( unsigned char /*datasets*/ ) {
}

void Renderer::InvalidateWindow() {
}

}
