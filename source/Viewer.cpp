#include "Viewer.h"

#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>

#include <QPainter>

Viewer::Viewer()
{
    QSurfaceFormat format;
    format.setSamples(4);
    setFormat(format);

    setMouseTracking(true);
}

void Viewer::initializeGL()
{
    initializeOpenGLFunctions();

    /// Antialiasing and alpha blending:
    glEnable(GL_MULTISAMPLE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);

    /// Prepare shaders:
    // Lines shader:
    {
        auto program = new QOpenGLShaderProgram (context());
        program->addShaderFromSourceCode(QOpenGLShader::Vertex,
            "attribute highp vec4 vertex;\n"
            "uniform highp mat4 matrix;\n"
            "varying vec3 pos;\n"
            "void main(void)\n"
            "{\n"
            "   gl_Position = matrix * vertex;\n"
            "   pos = vertex.xyz;\n"
            "}");
        program->addShaderFromSourceCode(QOpenGLShader::Fragment,
            "uniform mediump vec4 color;\n"
            "varying vec3 pos;\n"
            "void main(void)\n"
            "{\n"
            "   color.a = clamp(0.5 - color.a * length(pos),0,1);\n"
            "   gl_FragColor = color; \n"
            "   if (gl_FragColor.a < 0.01)discard;"
            "}");
        program->link();

        shaders.insert("lines", program);
    }

    // Box shader:
    {
        auto program = new QOpenGLShaderProgram (context());
        program->addShaderFromSourceCode(QOpenGLShader::Vertex,
            "attribute highp vec4 vertex;\n"
            "attribute highp vec4 color;\n"
            "uniform highp mat4 matrix;\n"
            "varying vec4 vColor;\n"
            "void main(void)\n"
            "{\n"
            "   gl_Position = matrix * vertex;\n"
            "   vColor = color;\n"
            "}");
        program->addShaderFromSourceCode(QOpenGLShader::Fragment,
            "varying vec4 vColor;\n"
            "void main(void)\n"
            "{\n"
            "   gl_FragColor = vColor;\n"
            "}");
        program->link();

        shaders.insert("box", program);
    }

    // Textured quad:
    {
        auto program = new QOpenGLShaderProgram (context());
        program->addShaderFromSourceCode(QOpenGLShader::Vertex,
            "attribute highp vec4 vertex;\n"
            "attribute highp vec2 texcoord;\n"
            "varying vec2 v_texCoords;\n"
            "void main(void)\n"
            "{\n"
            "   gl_Position = vertex;\n"
            "   v_texCoords = texcoord;\n"
            "}");
        program->addShaderFromSourceCode(QOpenGLShader::Fragment,
             "uniform sampler2D texture; \n"
             "varying vec2 v_texCoords;\n"
             "void main() \n"
             "{ \n"
             "    gl_FragColor = texture2D(texture, v_texCoords); \n"
             "}\n");
        program->link();

        shaders.insert("texturedQuad", program);
    }

    // Mesh
    {
        auto program = new QOpenGLShaderProgram (context());
        program->addShaderFromSourceCode(QOpenGLShader::Vertex,
            "#version 330 core\n"
            "layout (location = 0) in vec4 vertex;\n"
            "layout (location = 1) in vec4 normal;\n"
            "layout (location = 2) in vec4 color;\n"
            "uniform mat4 matrix;\n"
            "out vec3 FragPos;\n"
            "out vec3 Normal;\n"
            "out vec4 Color;\n"
            "void main(void)\n"
            "{\n"
            "   gl_Position = matrix * vertex;\n"
            "   FragPos = vertex.xyz;\n"
            "   Normal = normal.xyz;\n"
            "   Color = color;\n"
            "}");
        program->addShaderFromSourceCode(QOpenGLShader::Fragment,
            "#version 330 core\n"
            "out vec4 color;\n"
            "in vec3 FragPos;\n"
            "in vec3 Normal;\n"
            "in vec4 Color;\n"
            "uniform vec3 lightPos;\n"
            "uniform vec3 viewPos;\n"
            "uniform vec3 lightColor;\n"
            "void main(void)\n"
            "{\n"
            "    // Ambient \n"
            "    float ambientStrength = 0.2f; \n"
            "    vec3 ambient = ambientStrength * lightColor; \n"
            "    \n"
            "    // Diffuse \n"
            "    vec3 norm = normalize(Normal); \n"
            "    vec3 lightDir = normalize(lightPos - FragPos); \n"
            "    float diff = max(dot(norm, lightDir), 0.0); \n"
            "    vec3 diffuse = diff * lightColor; \n"
            "    \n"
            "    // Specular \n"
            "    vec3 fakeLightDir = normalize(vec3(0.5,0.5,1));\n"
            "    float specularStrength = 1.0f; \n"
            "    vec3 viewDir = normalize(viewPos - FragPos); \n"
            "    vec3 reflectDir = reflect(-fakeLightDir, norm);  \n"
            "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64); \n"
            "    vec3 specular = specularStrength * spec * lightColor;   \n"
            "    \n"
            "    vec3 result = (ambient + diffuse + specular) * Color.xyz; \n"
            "    color = vec4(result, 1.0f); \n"
            "}");
        program->link();

        shaders.insert("mesh", program);
    }
}

QMatrix4x4 Viewer::defaultOrthoViewMatrix(int type, int w, int h, float zoomFactor)
{
    QMatrix4x4 proj, view, pvMatrix;

    float ratio = float(w) / h;
    float orthoZoom = 1 + zoomFactor;
	float orthoEye = 10;

	// Default is top view
    QVector3D focalPoint(0, 0, 0);
	QVector3D eye(0, 0, orthoEye);
	QVector3D upVector(0.0, 1.0, 0.0);

	if (type == 1){ /* FRONT */
        eye = QVector3D(0, -orthoEye, 0);
		upVector = QVector3D(0,0,1);
	}

	if (type == 2){ /* LEFT */
		eye = QVector3D(-orthoEye, 0, 0);
		upVector = QVector3D(0,0,1);
	}
    
	view.lookAt(eye, focalPoint, upVector);
	proj.ortho(orthoZoom * -ratio, orthoZoom * ratio, orthoZoom * -1, orthoZoom * 1, 0.01f, 100.0f);

	pvMatrix = proj * view;
    return pvMatrix;
}

void Viewer::drawLines(const QVector< QVector3D > &lines, QColor color, QMatrix4x4 camera)
{
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    auto & program = *shaders["lines"];

    // Activate shader
    program.bind();

    int vertexLocation = program.attributeLocation("vertex");
    int matrixLocation = program.uniformLocation("matrix");
    int colorLocation = program.uniformLocation("color");

    // Pack geometry
    QVector<GLfloat> vertices;
    for(auto p : lines) {vertices.push_back(p.x()); vertices.push_back(p.y()); vertices.push_back(p.z());}

    // Shader data
    program.enableAttributeArray(vertexLocation);
    program.setAttributeArray(vertexLocation, &vertices[0], 3);
    program.setUniformValue(matrixLocation, camera);
    program.setUniformValue(colorLocation, color);

    // Draw lines
    glDrawArrays(GL_LINES, 0, vertices.size() / 3);

    program.disableAttributeArray(vertexLocation);

    program.release();

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}

void Viewer::drawBox(double width, double length, double height, QMatrix4x4 camera)
{
    glEnable( GL_DEPTH_TEST );

    // Define the 8 vertices of a unit cube
    GLfloat g_Vertices[24] = {
          0.5,  0.5,  1,
         -0.5,  0.5,  1,
         -0.5, -0.5,  1,
          0.5, -0.5,  1,
          0.5, -0.5, 0,
         -0.5, -0.5, 0,
         -0.5,  0.5, 0,
          0.5,  0.5, 0
    };

    GLfloat g_Colors[24] = {
         1, 1, 1,
         0, 1, 1,
         0, 0, 1,
         1, 0, 1,
         1, 0, 0,
         0, 0, 0,
         0, 1, 0,
         1, 1, 0,
    };

    // Define the vertex indices for the cube.
    GLuint g_Indices[24] = {
        0, 1, 2, 3,                 // Front face
        7, 4, 5, 6,                 // Back face
        6, 5, 2, 1,                 // Left face
        7, 0, 3, 4,                 // Right face
        7, 6, 1, 0,                 // Top face
        3, 2, 5, 4,                 // Bottom face
    };

    //glDrawElements( GL_QUADS, 24, GL_UNSIGNED_INT, &g_Indices[0] );

    // Pack geometry & colors
    QVector<GLfloat> vertices, colors;
    for(int i = 0; i < 24; i++)
    {
        int v = g_Indices[i] * 3;

        vertices.push_back(width * g_Vertices[v+0]);
        vertices.push_back(length * g_Vertices[v+1]);
        vertices.push_back(height * g_Vertices[v+2]);

        colors.push_back(g_Colors[v+0]);
        colors.push_back(g_Colors[v+1]);
        colors.push_back(g_Colors[v+2]);
    }

    auto & program = *shaders["box"];

    // Activate shader
    program.bind();

    int vertexLocation = program.attributeLocation("vertex");
    int colorLocation = program.attributeLocation("color");
    int matrixLocation = program.uniformLocation("matrix");

    // Shader data
    program.enableAttributeArray(vertexLocation);
    program.setAttributeArray(vertexLocation, &vertices[0], 3);
    program.enableAttributeArray(colorLocation);
    program.setAttributeArray(colorLocation, &colors[0], 3);

    program.setUniformValue(matrixLocation, camera);

    // Draw faces
    glDrawArrays(GL_QUADS, 0, 24);

    program.disableAttributeArray(vertexLocation);
    program.disableAttributeArray(colorLocation);

    program.release();

    glDisable( GL_DEPTH_TEST );
}

void Viewer::drawQuad(const QImage & img)
{
    QOpenGLTexture texture(img.mirrored());
    texture.setMinificationFilter(QOpenGLTexture::LinearMipMapLinear);
    texture.setMagnificationFilter(QOpenGLTexture::Linear);
    texture.bind();

    auto & program = *shaders["texturedQuad"];

    // Activate shader
    program.bind();

    // Draw quad
    float f = 1.0;
    GLfloat g_Vertices[] = {-f,  f ,0,
                            -f, -f ,0,
                             f,  f ,0,
                             f, -f ,0};

    GLfloat g_Texcoord[] = {0, 1,
                            0, 0,
                            1, 1,
                            1, 0};

    GLubyte g_Indices[] = {0,1,2, // first triangle (bottom left - top left - top right)
                           2,1,3}; // second triangle (bottom left - top right - bottom right)

    QVector<GLfloat> vertices, texcoord;
    for(int i = 0; i < 6; i++)
    {
        int v = g_Indices[i] * 3;
        vertices.push_back(g_Vertices[v+0]);
        vertices.push_back(g_Vertices[v+1]);
        vertices.push_back(g_Vertices[v+2]);

        int vt = g_Indices[i] * 2;
        texcoord.push_back(g_Texcoord[vt+0]);
        texcoord.push_back(g_Texcoord[vt+1]);
    }

    int vertexLocation = program.attributeLocation("vertex");
    int textureLocation = program.attributeLocation("texcoord");

    // Shader data
    program.enableAttributeArray(vertexLocation);
    program.setAttributeArray(vertexLocation, &vertices[0], 3);
    program.enableAttributeArray(textureLocation);
    program.setAttributeArray(textureLocation, &texcoord[0], 2);


    // Draw quad
    glDrawArrays(GL_TRIANGLES, 0, 6);

    program.release();

    texture.release();
}


