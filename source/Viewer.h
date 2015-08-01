#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_2_Core>
#include <QOpenGLShaderProgram>
#include <QVector3D>
#include <QMatrix4x4>

class Viewer : public QOpenGLWidget, public QOpenGLFunctions_3_2_Core
{
public:
    Viewer();
    void initializeGL();

    QMap<QString, QOpenGLShaderProgram*> shaders;
    QMatrix4x4 pvm;
    QVector3D eyePos;

    // Draw primitives
    void drawLines(const QVector<QVector3D> &lines, QColor color, QMatrix4x4 camera);
    void drawBox(double width, double length, double height, QMatrix4x4 camera);
    void drawQuad(const QImage &img);

    // Camera stuff:
    static QMatrix4x4 defaultOrthoViewMatrix(int type, int w, int h);
	static QMatrix4x4 defaultOrthoViewMatrix(int type, int w, int h, float zoomFactor = 1.0f);
};
