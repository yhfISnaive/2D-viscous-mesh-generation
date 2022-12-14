
#include <igl/arap.h>
#include <igl/boundary_loop.h>
#include <igl/harmonic.h>
#include <igl/map_vertices_to_circle.h>
#include <igl/readOBJ.h>
#include <igl/Timer.h>
#include <igl/opengl/glfw/Viewer.h>
#include <igl/MappingEnergyType.h>
#include <igl/doublearea.h>
#include <igl/PI.h>
#include <igl/flipped_triangles.h>
#include <igl/topological_hole_fill.h>
#include <igl/winding_number.h>
#include <igl/writeSTL.h>
#include <igl/list_to_matrix.h>
#include <igl/cat.h>

#include "../include/normalprismatic.h"
#include "../include/rigidmapping.h"
#include "../include/readDAT.h"
#include "../include/readSTLbnd.h"
#include "../include/loops.h"
#include "../include/writeVTK.h"
#include "../include/multiplenormal.h"

#include "../third/CLI11/include/CLI/CLI.hpp"

using namespace RIGIDT;
Eigen::MatrixXd V;
Eigen::MatrixXi F;
Eigen::MatrixXd V_uv;
Eigen::MatrixXd V_tri;
Eigen::MatrixXi F_tri;
igl::Timer timer;
RIGIDT::SCAFData hybrid_data;

bool show_uv = false;
bool show_whole = false;
float uv_scale = 1.0f;

bool key_down(igl::opengl::glfw::Viewer& viewer, unsigned char key, int modifier)
{
    if (key == '1')
        show_uv = false;
    else if (key == '2')
        show_uv = true;
    else if (key == '3')
        show_whole = !show_whole;
    else if (key == 's') {
   
    }


    if (key == ' ')
    {
        timer.start();

        RIGIDT::scaf_solve(hybrid_data, 1);
        std::cout << "time = " << timer.getElapsedTime() << std::endl;
    }

    const auto& V_uv = uv_scale * hybrid_data.w_uv.topRows(V.rows());
    const auto& V_uv_whole = uv_scale * hybrid_data.w_uv;

    if (show_uv)
    {

        Eigen::MatrixXi Topo;
        igl::cat(1,F, hybrid_data.s_T, Topo);
        viewer.data().clear();
        if (show_whole) {
            viewer.data().set_mesh(V_uv_whole, Topo);
            viewer.data().set_uv(V_uv_whole);



        }
        else {
            viewer.data().set_mesh(V_uv, F);
            viewer.data().set_uv(V_uv);
        }
        
    }
    else
    {
        viewer.data().clear();
        viewer.data().set_mesh(hybrid_data.target_V, hybrid_data.target_F);
        viewer.data().set_uv(V_uv);
       // viewer.core().align_camera_center(V, F);
    }

    viewer.data().compute_normals();

    return false;
}
int main(int argc, char* argv[])
{
	CLI::App app("2Dexpansion");

	std::string input_filename;
	double first_length=0.01;
	double ratio=1.15;
	int max_iteration = 100;
    double preserving_layer_height = 1;// 1 means by default, 0.5 means half of the default value, so as on 
    bool use_multiple_normal = false;//optional


	app.description("A 2d viscous mesh generator.");
	app.add_option("-i", input_filename, "input filename. (string, required, supported format: wrl)")->required();
	app.add_option("-r", ratio, "the expansion ratio");
	app.add_option("-f", first_length, "the height of the first layer.");
	app.add_option("-I", max_iteration, "the maximum times of iteration.");
    app.add_option("-p", preserving_layer_height, "the height ratio of presering layer: e.g. 1 means the default value, and 0.2 means half of the default value.");
    app.add_option("--multiple", use_multiple_normal, "whether use multiple normal or not.");

	/// prase the input command by CLI
	try {
		app.parse(argc, argv);
	}
	catch (const CLI::ParseError &e) {
		return app.exit(e);
	}


	

    using namespace std;
    // Load a mesh in OFF format

    Eigen::MatrixXd V1;
    Eigen::MatrixXi F1;
    Eigen::MatrixXd V_uvp;
    Eigen::MatrixXi F_uvp;
    RIGIDT::readSTLbnd(input_filename,V1,F1);

    
    auto L=RIGIDT::findLoop(F1);
    auto loop_group=RIGIDT::orientLoop(L, V1);

    Eigen::MatrixXd V2;
    Eigen::MatrixXi F2;
    RIGIDT::reoriganize(loop_group,V1,V2,F2);

    int count = 0;
    if(use_multiple_normal)
	RIGIDT::VirtualMeshGeneration(V2,F2,V2,F2);
    



    //igl::readOBJ(string(PA)+"/camel_b.obj", V, F);
    hybrid_data.mesh=std::shared_ptr<NormalPrismaticMesh>(new NormalPrismaticMesh(V2, F2, first_length, ratio));
	//  if (use_multiple_normal) {
	//hybrid_data.mesh->setMultipleNormal();
	//  }


    hybrid_data.mesh->setPreservingLayer(preserving_layer_height);
    hybrid_data.input_point_num = V2.rows();

    Eigen::MatrixXd Discre_V;
    Eigen::MatrixXi Discre_F;

	Eigen::VectorXd scale; std::vector<int> discreate;
    hybrid_data.mesh->getCylinderMesh(V,F,scale);
    hybrid_data.mesh->getDiscreteCylinderMesh(Discre_V, Discre_F,scale,discreate);

    hybrid_data.mesh->getUVMesh(V_uvp, F_uvp);


   


    hybrid_data.mesh->getHoles(loop_group, V1, hybrid_data.component_sizes);

    Eigen::MatrixXd bnd_uv, uv_init;

    Eigen::VectorXd M;
    igl::doublearea(V, F, M);
    hybrid_data.all_bnds = hybrid_data.mesh->getAllBound();

   

    V_uvp.conservativeResize(V.rows(), 2);
    uv_init = V_uvp;
    std::vector<int> fix_id(V1.size());
    for (int i = 0; i < fix_id.size(); i++) { fix_id[i] = i; }
    igl::list_to_matrix(fix_id, hybrid_data.fixed_ids);
    
    


    Eigen::VectorXi b; Eigen::MatrixXd bc;
    RIGIDT::scaf_precompute(V,F,Discre_V, Discre_F, uv_init, hybrid_data, igl::MappingEnergyType::SYMMETRIC_DIRICHLET, b, bc, 0);



    RIGIDT::scaf_solve(hybrid_data, max_iteration);
    const auto& V_uv = uv_scale * hybrid_data.w_uv.topRows(V.rows());
    Eigen::MatrixXd V_final = hybrid_data.w_uv.topRows(V.rows());
    hybrid_data.mesh->saveVTK("quad.vtk", V_final);

    Eigen::MatrixXd V_edge; RIGIDT::LoopGroup lg;
    hybrid_data.mesh->getTopLoop(loop_group, V1, V_uv, V_edge, lg);
    hybrid_data.mesh->generateOuterTriMesh(lg,V_edge,V_tri, F_tri);

    RIGIDT::writeVTK("tri.vtk",V_tri, F_tri);
    RIGIDT::writeVTK("tri_triangle.vtk", hybrid_data.w_uv, hybrid_data.s_T);
    
    // Plot the mesh
    igl::opengl::glfw::Viewer viewer;
    viewer.data().set_mesh(Discre_V, Discre_F);
    viewer.core().background_color= Eigen::Vector4f(1,1,1,1);
   
    viewer.data().set_uv(V_uv);
    viewer.callback_key_down = &key_down;

    // Enable wireframe
    viewer.data().show_lines = true; 

    // Draw checkerboard texture
    viewer.data().show_texture = true;


    std::cerr << "Press space for running an iteration." << std::endl;
    std::cerr << "Press 1 for Mesh 2 for UV" << std::endl;

    // Launch the viewer
    viewer.launch();
}
