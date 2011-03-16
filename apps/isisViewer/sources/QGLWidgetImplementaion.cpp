#include "QGLWidgetImplementation.hpp"
#include <QVBoxLayout>
#include <QMouseEvent>

namespace isis
{
namespace viewer
{


QGLWidgetImplementation::QGLWidgetImplementation( ViewerCore *core, QWidget *parent, QGLWidget *share, OrientationHandler::PlaneOrientation orientation )
	: QGLWidget( parent, share ),
	  m_ViewerCore( core ),
	  m_PlaneOrientation( orientation ),
	  m_ShareWidget( share )
{
	( new QVBoxLayout( parent ) )->addWidget( this );
	commonInit();
}

QGLWidgetImplementation::QGLWidgetImplementation( ViewerCore *core, QWidget *parent, OrientationHandler::PlaneOrientation orientation )
	: QGLWidget( parent ),
	  m_ViewerCore( core ),
	  m_PlaneOrientation( orientation )
{
	( new QVBoxLayout( parent ) )->addWidget( this );
	commonInit();
}


void QGLWidgetImplementation::commonInit()
{
	setSizePolicy( QSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored ) );
	setMouseTracking( true );
	connectSignals();
}

QGLWidgetImplementation* QGLWidgetImplementation::createSharedWidget( QWidget *parent, OrientationHandler::PlaneOrientation orientation )
{
	return new QGLWidgetImplementation( m_ViewerCore, parent, this, orientation );
}


void QGLWidgetImplementation::connectSignals()
{
	connect( this, SIGNAL( redraw() ), SLOT( updateGL() ) );
}

void QGLWidgetImplementation::initializeGL()
{
	LOG( Debug, verbose_info ) << "initializeGL " << objectName().toStdString();
	util::Singletons::get<GLTextureHandler, 10>().copyAllImagesToTextures( m_ViewerCore->getDataContainer() );
	paintVolume(0,0,85, false);	
}


void QGLWidgetImplementation::resizeGL(int w, int h)
{
	LOG(Debug, verbose_info) << "resizeGL " << objectName().toStdString();
}


bool QGLWidgetImplementation::paintVolume( size_t imageID, size_t timestep, size_t slice, bool redrawFlag )
{

	//TODO this is only a prove of concept. has to be structured and optimized!!
	
	//check if the image is available at all
	if(!m_ViewerCore->getDataContainer().isImage(imageID,timestep, slice))
	{
		LOG( Runtime, error ) << "Tried to paint image with id " << imageID << " and timestep " << timestep << 
			", but no such image exists!";
		return false;
	}
	
	//copy the volume to openGL. If this already has happend GLTextureHandler does nothing.
	GLuint textureID = util::Singletons::get<GLTextureHandler, 10>().copyImageToTexture( m_ViewerCore->getDataContainer(), imageID, timestep );
	ImageHolder image = m_ViewerCore->getDataContainer()[imageID];
	OrientationHandler::MatrixType orient =  OrientationHandler::getOrientationMatrix(image, m_PlaneOrientation, true );
	float matrix[16];
	OrientationHandler::boostMatrix2Pointer(  OrientationHandler::orientation2TextureMatrix( orient ), matrix );

// 	std::cout << objectName().toStdString() << std::endl;
// 	OrientationHandler::printMatrix(matrix);

	glMatrixMode(GL_TEXTURE);
// 	glLoadIdentity();
	glLoadMatrixf( matrix );
	glEnable( GL_TEXTURE_3D );
	glBindTexture( GL_TEXTURE_3D, textureID );
	glBegin( GL_QUADS );
	glTexCoord3f( 0, 0, 0.5 );
	glVertex2f( -1.0, -1.0 ); 
	glTexCoord3f( 0, 1, 0.5);
	glVertex2f( -1.0, 1.0 );
	glTexCoord3f( 1, 1, 0.5 );
	glVertex2f( 1.0, 1.0 );
	glTexCoord3f( 1, 0, 0.5);
	glVertex2f( 1.0, -1.0 );		
	glEnd();
	glFlush();
	glDisable( GL_TEXTURE_3D );
	if(redrawFlag) {
		redraw();
	}
}


void QGLWidgetImplementation::mouseMoveEvent( QMouseEvent *e )
{
	//TODO debug
// 	float pos = float( e->x() ) / geometry().width();
	paintVolume(0,0,e->x());

}





}
} // end namespace