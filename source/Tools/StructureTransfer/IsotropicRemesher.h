#pragma once

#include "SurfaceMeshModel.h"
#include "MathHelper.h"

using namespace opengp::SurfaceMesh; // be careful

#include "NanoKdTree.h"

namespace Remesh{

struct IsotropicRemesher{

    SurfaceMeshModel * mesh;
    Vector3VertexProperty points;
    BoolEdgeProperty efeature;
    NanoKdTree kdtree;

    IsotropicRemesher(SurfaceMeshModel * mesh) : mesh(mesh){}

    void remesh(double targetEdgeLength, int numIterations, bool isProjectSurface, bool isKeepShortEdges)
    {
        const double low  = (4.0 / 5.0) * targetEdgeLength;
        const double high = (4.0 / 3.0) * targetEdgeLength;

        // Copy original mesh
        auto copy = mesh->clone();

        // Build KD-tree
        kdtree.cloud.pts.clear();
        for(auto v : mesh->vertices()) kdtree.addPoint(points[v]);
        kdtree.build();

        for(int i = 0; i < numIterations; i++)
        {
            splitLongEdges(high);
            collapseShortEdges(low, high, isKeepShortEdges);
            equalizeValences();
            tangentialRelaxation();
            if(isProjectSurface) projectToSurface( copy );
        }

        delete copy;
    }

    /// performs edge splits until all edges are shorter than the threshold
    void splitLongEdges(double maxEdgeLength )
    {
        const double maxEdgeLengthSqr = maxEdgeLength * maxEdgeLength;

        // iterate over all edges
        for (auto e_it : mesh->edges())
        {
            const SurfaceMeshModel::Halfedge & hh = mesh->halfedge( e_it, 0 );

            const SurfaceMeshModel::Vertex & v0 = mesh->from_vertex(hh);
            const SurfaceMeshModel::Vertex & v1 = mesh->to_vertex(hh);

            Vector3 vec = points[v1] - points[v0];

            // edge to long?
            if ( vec.squaredNorm() > maxEdgeLengthSqr ){

                const Vector3 midPoint = points[v0] + ( 0.5 * vec );

                // split at midpoint
                SurfaceMeshModel::Vertex vh = mesh->add_vertex( midPoint );

                bool hadFeature = efeature[e_it];

                mesh->split(e_it, vh);

                if ( hadFeature )
                {
                    for(auto e : mesh->halfedges(vh))
                    {
                        if ( mesh->to_vertex(e) == v0 || mesh->to_vertex(e) == v1 )
                        {
                            efeature[mesh->edge(e)] = true;
                        }
                    }
                }
            }
        }
    }

    /// collapse edges shorter than minEdgeLength if collapsing doesn't result in new edge longer than maxEdgeLength
    void collapseShortEdges(const double _minEdgeLength, const double _maxEdgeLength, bool isKeepShortEdges )
    {
        const double _minEdgeLengthSqr = _minEdgeLength * _minEdgeLength;
        const double _maxEdgeLengthSqr = _maxEdgeLength * _maxEdgeLength;

        //add checked property
        BoolEdgeProperty checked = mesh->edge_property< bool >("e:checked", false);

        bool finished = false;

        while( !finished ){

            finished = true;

            for (auto e_it : mesh->edges())
            {
                if ( checked[e_it] )
                    continue;

                checked[e_it] = true;

                auto hh = mesh->halfedge( e_it, 0 );

                auto v0 = mesh->from_vertex(hh);
                auto v1 = mesh->to_vertex(hh);

                const Vector3 vec = points[v1] - points[v0];

                const double edgeLength = vec.squaredNorm();

                // Keep originally short edges, if requested
                bool hadFeature = efeature[e_it];
                if ( isKeepShortEdges && hadFeature ) continue;

                // edge too short but don't try to collapse edges that have length 0
                if ( (edgeLength < _minEdgeLengthSqr) && (edgeLength > std::numeric_limits<double>::epsilon()) ){

                    //check if the collapse is ok
                    const Vector3 & B = points[v1];

                    bool collapse_ok = true;

                    for( auto hvit : mesh->halfedges(v0) )
                    {
                        double d = (B - points[ mesh->to_vertex(hvit) ]).squaredNorm();

                        if ( d > _maxEdgeLengthSqr || mesh->is_boundary( mesh->edge( hvit ) ) || efeature[mesh->edge(hvit)] )
                        {
                            collapse_ok = false;
                            break;
                        }
                    }

                    if( collapse_ok && mesh->is_collapse_ok(hh) ) {
                        mesh->collapse( hh );

                        finished = false;
                    }
                }
            }
        }

        mesh->remove_edge_property(checked);

        mesh->garbage_collection();
    }

    void equalizeValences()
    {
        for (auto e_it : mesh->edges())
        {
            if ( !mesh->is_flip_ok(e_it) ) continue;
            if ( efeature[e_it] ) continue;

            auto h0 = mesh->halfedge( e_it, 0 );
            auto h1 = mesh->halfedge(e_it, 1);

            if (h0.is_valid() && h1.is_valid())
            {
                if (mesh->face(h0).is_valid() && mesh->face(h1).is_valid()){
                    //get vertices of corresponding faces
                    auto a = mesh->to_vertex(h0);
                    auto b = mesh->to_vertex(h1);
                    auto c = mesh->to_vertex(mesh->next_halfedge(h0));
                    auto d = mesh->to_vertex(mesh->next_halfedge(h1));

                    const int deviation_pre =  abs((int)(mesh->valence(a) - targetValence(a)))
                            +abs((int)(mesh->valence(b) - targetValence(b)))
                            +abs((int)(mesh->valence(c) - targetValence(c)))
                            +abs((int)(mesh->valence(d) - targetValence(d)));
                    mesh->flip(e_it);

                    const int deviation_post = abs((int)(mesh->valence(a) - targetValence(a)))
                            +abs((int)(mesh->valence(b) - targetValence(b)))
                            +abs((int)(mesh->valence(c) - targetValence(c)))
                            +abs((int)(mesh->valence(d) - targetValence(d)));

                    if (deviation_pre <= deviation_post)
                        mesh->flip(e_it);
                }
            }
        }
    }

    ///returns 4 for boundary vertices and 6 otherwise
    inline int targetValence(const SurfaceMeshModel::Vertex& _vh ){
        if (isBoundary(_vh))
            return 4;
        else
            return 6;
    }

    inline bool isBoundary(const SurfaceMeshModel::Vertex& _vh )
    {
        for( auto hvit : mesh->halfedges(_vh) )
        {
            if ( mesh->is_boundary( mesh->edge( hvit ) ) )
                return true;
        }
        return false;
    }

    inline bool isFeature(const SurfaceMeshModel::Vertex& _vh )
    {
        for (auto hvit : mesh->halfedges(_vh))
        {
            if(efeature[mesh->edge(hvit)])
                return true;
        }

        return false;
    }

    void tangentialRelaxation(  )
    {
        mesh->update_face_normals();
        mesh->update_vertex_normals();

        auto q = mesh->vertex_property<Vector3>("v:q");
        auto normal = mesh->vertex_property<Vector3>(VNORMAL);

        //first compute barycenters
        for (auto v_it : mesh->vertices())
        {
            Vector3 tmp(0,0,0);
            uint N = 0;

            for (auto hvit : mesh->halfedges(v_it))
            {
                tmp += points[ mesh->to_vertex(hvit) ];
                N++;
            }

            if (N > 0)
                tmp /= (double) N;

            q[v_it] = tmp;
        }

        //move to new position
        for (auto v_it : mesh->vertices())
        {
            if ( !isBoundary(v_it) && !isFeature(v_it) )
            {
                //Vector3 newPos = q[v_it] + (dot(normal[v_it], (points[v_it] - q[v_it]) ) * normal[v_it]);
                points[v_it] = q[v_it];
            }
        }

        mesh->remove_vertex_property(q);
    }

    Vector3 findNearestPoint(SurfaceMeshModel * original_mesh, const Vector3& _point, SurfaceMeshModel::Face& _fh, double* _dbest)
    {
        Vector3VertexProperty orig_points = original_mesh->vertex_property<Vector3>( VPOINT );

        double fc = original_mesh->bbox().diagonal().norm() * 2;
        Vector3  p_best = Vector3(fc,fc,fc) + Vector3(original_mesh->bbox().center());
        double d_best = (_point - p_best).squaredNorm();
        SurfaceMeshModel::Face fh_best;

        bool isExhaustiveSearch = false;

        if( isExhaustiveSearch )
        {
            for(auto f : original_mesh->faces())
            {
                auto cfv_it = original_mesh->vertices(f);

                // Assume triangular
                const Vector3& pt0 = orig_points[   *(cfv_it)];
                const Vector3& pt1 = orig_points[ *(++cfv_it)];
                const Vector3& pt2 = orig_points[ *(++cfv_it)];

                Vector3 ptn = _point;

                //SurfaceMeshModel::Scalar d = distPointTriangleSquared( _point, pt0, pt1, pt2, ptn );
                auto d = ClosestPointTriangle( _point, pt0, pt1, pt2, ptn );

                if( d < d_best)
                {
                    d_best = d;
                    p_best = ptn;

                    fh_best = f;
                }
            }
        }
        else
        {
            KDResults matches;
            kdtree.k_closest(_point, 32, matches);

            for(auto match : matches)
            {
                for(auto h : original_mesh->halfedges(SurfaceMeshModel::Vertex((int)match.first)))
                {
                    auto f = original_mesh->face(h);
                    if(!mesh->is_valid(f)) continue;

                    SurfaceMeshModel::Vertex_around_face_circulator cfv_it = original_mesh->vertices( f );

                    // Assume triangular
                    const Vector3& pt0 = orig_points[   *(cfv_it)];
                    const Vector3& pt1 = orig_points[ *(++cfv_it)];
                    const Vector3& pt2 = orig_points[ *(++cfv_it)];

                    Vector3 ptn = _point;

                    auto d = ClosestPointTriangle( _point, pt0, pt1, pt2, ptn );

                    if( d < d_best)
                    {
                        d_best = d;
                        p_best = ptn;

                        fh_best = f;
                    }
                }
            }
        }

        // return face
        _fh = fh_best;

        // return distance
        if(_dbest) *_dbest = sqrt(d_best);

        return p_best;
    }

    void projectToSurface(SurfaceMeshModel * orginal_mesh )
    {
        for (auto v_it : mesh->vertices())
        {
            if (isBoundary(v_it)) continue;
            if (isFeature(v_it)) continue;

            Vector3 p = points[v_it];

            SurfaceMeshModel::Face fhNear;
            double distance;

            Vector3 pNear = findNearestPoint(orginal_mesh, p, fhNear, &distance);

            points[v_it] = pNear;
        }
    }

    void apply(double longest_edge_length = -1, int num_split_iters = 10, bool is_project_surface = false,
               bool is_sharp_feature = true, double sharp_feature_angle = 44.0, bool is_keep_short_edges = false)
    {
        points = mesh->vertex_property<Vector3>(VPOINT);
        efeature = mesh->edge_property<bool>("e:feature", false);

        mesh->updateBoundingBox();
        double edgelength_TH = 0.02 * mesh->bbox().diagonal().norm();

        longest_edge_length = longest_edge_length < 0 ? edgelength_TH : longest_edge_length;

        // Prepare for sharp features
        if (is_sharp_feature){
            double angleDeg = sharp_feature_angle;
            double angleThreshold = deg_to_rad(angleDeg);

            for(auto e : mesh->edges())
            {
                if (abs(calc_dihedral_angle(*mesh, mesh->halfedge(e,0))) > angleThreshold)
                    efeature[e] = true;
            }
        }

        // Prepare for short edges on original mesh
        if (is_keep_short_edges){
            for(auto e : mesh->edges()){
                auto hh = mesh->halfedge( e, 0 );
                auto v0 = mesh->from_vertex(hh);
                auto v1 = mesh->to_vertex(hh);
                Vector3 vec = points[v1] - points[v0];

                if (vec.norm() <= longest_edge_length)
                    efeature[e] = true;
            }
        }

        /// Perform refinement
        this->remesh(longest_edge_length, num_split_iters, is_project_surface, is_keep_short_edges);

        // Clean up
        mesh->remove_edge_property(efeature);
    }
};
}

