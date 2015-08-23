#pragma once

#include <QVector3D>
#include <QStack>
#include "weld.h"

template<class Vector3> QVector3D toQVector3D(Vector3 p){ return QVector3D(p[0], p[1], p[2]); }

namespace GeometryHelper{
static inline double getSqSegDist(QVector3D p, QVector3D p1, QVector3D p2){
    float x = p1[0], y = p1[1], z = p1[2];
    float dx = p2[0] - x, dy = p2[1] - y, dz = p2[2] - z;
    if (dx != 0 || dy != 0 || dz != 0) {
        float t = ((p[0] - x) * dx + (p[1] - y) * dy) + (p[2] - z) * dz
                / (dx * dx + dy * dy + dz * dz);
        if (t > 1) {
            x = p2[0];
            y = p2[1];
            z = p2[2];
        } else if (t > 0) {
            x += dx * t;
            y += dy * t;
            z += dz * t;
        }
    }
    dx = p[0] - x;
    dy = p[1] - y;
    dz = p[2] - z;
    return dx * dx + dy * dy + dz * dz;
}

// simplification using optimized Douglas-Peucker algorithm without recursion
// based on a port of 'Simplify.js' by Vladimir Agafonkin
static inline QVector<QVector3D> simplifyDouglasPeucker(QVector<QVector3D> points, double tolerance){
    if(points.size() < 3) return points;

    QVector<int> keep(points.size());
    keep.front() = 1;
    keep.back() = 1;

    QStack< QPair<int,int> > stack;
    stack.push(QPair<int,int>(0, points.size() - 1));

    double sqTolerance = tolerance * tolerance;

    while(!stack.empty())
    {
        auto range = stack.pop();
        int first = range.first;
        int last = range.second;

        double maxSqDist = 0;
        int index = 0;

        for (int i = first + 1; i < last; i++) {
            double sqDist = getSqSegDist(points[i], points[first], points[last]);
            if (sqDist > maxSqDist) {
                index = i;
                maxSqDist = sqDist;
            }
        }
        if (maxSqDist > sqTolerance) {
            keep[index] = 1;
            stack.push(QPair<int,int>(first,index));
            stack.push(QPair<int,int>(index,last));
        }
    }

    QVector<QVector3D> newPoints;
    for (int i = 0; i < points.size(); i++) {
        if (keep[i]) {
            newPoints << points[i];
        }
    }
    return newPoints;
}

// based on code by Kenshi Takayama
static inline QVector<QVector3D> uniformResampleCount (QVector<QVector3D> points,
                                                       int target_num_points, bool is_loop = false){
    QVector<QVector3D> result(target_num_points);

    // Boundary cases
    int NN = points.size();
    if (NN < 2 || target_num_points < 2) return points;
    if (target_num_points == 2) {
        result << points.front() << points.back();
        return result;
    }

    // Closed curve?
    if (is_loop) {
        points.push_back(points.front());
        ++NN;
    }

    // Compute lengths
    std::vector<double> src_len_acc(NN, 0);
    for (int i = 1; i < NN; ++i)
        src_len_acc[i] = src_len_acc[i - 1] + (points[i] - points[i - 1]).length();

    // Re-sample
    double tgt_len_segment = src_len_acc.back() / (target_num_points - 1.);
    int src_i = 0;
    double tgt_len_acc = 0;
    int index = 0;
    result[index++] = points.front();
    while (true) {
        while (tgt_len_acc + tgt_len_segment <= src_len_acc[src_i]) {
            tgt_len_acc += tgt_len_segment;
            double w1 = (tgt_len_acc - src_len_acc[src_i - 1]) / (src_len_acc[src_i] - src_len_acc[src_i - 1]);
            double w0 = 1 - w1;
            auto p0 = points[src_i - 1];
            auto p1 = points[src_i];
            auto p = p0 * w0 + p1 * w1;
            result[index++] = p;
            if (index == target_num_points - 1) break;
        }

        if (index == target_num_points - 1) break;

        while (src_len_acc[src_i] <= tgt_len_acc + tgt_len_segment) {
            ++src_i;
        }
    }
    result[index++] = points.back();

    // Closed poly
    if (is_loop) result.removeLast();

    return result;
}

static inline QVector<QVector3D> smooth(QVector<QVector3D> points, int num_iter = 1, bool is_loop = false,
                                        double weight_first_order = 1.0, double weight_second_order = 0.0, double damping = 0.5){
    auto size = points.size();

    for (int k = 0; k < num_iter; ++k) {
        auto points_old = points;
        for (int i = 0; i < size; ++i) {
            if (!is_loop && i == 0 || i == size - 1)
                continue;

            int i_next  = (i + 1) % size;
            int i_prev  = (i + size - 1) % size;
            int i_next2 = (i + 2) % size;
            int i_prev2 = (i + size - 2) % size;

            auto p_prev  = points_old[i_prev ];
            auto p_next  = points_old[i_next ];
            auto p_prev2 = points_old[i_prev2];
            auto p_next2 = points_old[i_next2];

            auto p_first_order  = (p_prev + p_next) / 2.0;
            auto p_second_order = (-p_prev2 + 4.0 * p_prev + 4.0 * p_next - p_next2) / 6.0;

            if (!is_loop && i == 1 || i == size - 2)
                p_second_order = p_first_order;  // undefined

            auto p_target = (weight_first_order * p_first_order +
                             weight_second_order * p_second_order)
                    / (weight_first_order + weight_second_order);

            points[i] = damping * points_old[i] + (1 - damping) * p_target;
        }
    }

    return points;
}

static inline QVector3D pointAt(QVector<QVector3D> points, double arc_length_parameter){
    if(points.empty()) return QVector3D();

    if (arc_length_parameter <= 0) return points.front();
    if (arc_length_parameter >= 1) return points.back();

    auto length = [&](QVector<QVector3D> points){
        double result = 0;
        for(int i = 1; i < points.size(); i++)
            result += (points[i] - points[i-1]).length();
        return result;
    };

    const int NN = points.size();
    const double target_length = length(points) * arc_length_parameter;

    double length_acc = 0;
    for (int i = 0; i < NN; ++i) {
        if (i == NN - 1) break;

        auto& p0 = points[i];
        auto& p1 = points[(i + 1) % NN];

        double segment_length = (p1 - p0).length();
        if (length_acc <= target_length && target_length < length_acc + segment_length) {
            double t = (target_length - length_acc) / segment_length;
            return (1 - t) * p0 + t * p1;
        }

        length_acc += segment_length;
    }

    // something wrong happened
    return QVector3D();
}

template<class Vector3, class Mesh>
inline void meregeVertices(Mesh * m){
	std::vector<Vector3> vertices;
	auto points = m->vertex_coordinates();
	for (auto v : m->vertices()) vertices.push_back(points[v]);

	std::vector<size_t> xrefs;
	weld(vertices, xrefs, std::hash_Vector3<Vector3>(), [&](const Vector3& a, const Vector3& b){ return a.isApprox(b); });

	std::vector< std::vector<Mesh::Vertex> > faces;
	for (auto f : m->faces()){
		std::vector<Mesh::Vertex> face;
		for (auto v : m->vertices(f)) face.push_back(Mesh::Vertex(xrefs[v.idx()]));
		faces.push_back(face);
	}

	m->clear();

	for (auto v : vertices) m->add_vertex(v);
	for (auto face : faces) m->add_face(face);
}

template<class Vector3>
inline static bool intersectRayTri(const std::vector<Vector3> & tri, const Vector3 & rayOrigin,
    const Vector3 & rayDirection, Vector3 & intersectionPoint)
{
    double u, v, t;
    Vector3 edge1 = tri[1] - tri[0];
    Vector3 edge2 = tri[2] - tri[0];
    Vector3 pvec = rayDirection.cross(edge2);
    double det = edge1.dot(pvec);
    if (det == 0) return false;
    double invDet = 1 / det;
    Vector3 tvec = rayOrigin - tri[0];
    u = tvec.dot(pvec) * invDet;
    if (u < 0 || u > 1) return false;
    Vector3 qvec = tvec.cross(edge1);
    v = rayDirection.dot(qvec) * invDet;
    if (v < 0 || u + v > 1) return false;
    t = edge2.dot(qvec) * invDet;
    intersectionPoint = rayOrigin + (t * rayDirection);
    return true;
}

}
