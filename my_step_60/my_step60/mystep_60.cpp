#include <deal.II/base/logstream.h>
#include <deal.II/base/utilities.h>
#include <deal.II/base/timer.h>

// define public parameter
#include <deal.II/base/parameter_acceptor.h>
#include <deal.II/grid/tria.h>

#include <deal.II/grid/grid_generator.h>
#include <deal.II/grid/grid_tools.h>

// make it possible to save stuff that was calculated for later use in the code
#include <deal.II/grid/grid_tools_cache.h>


#include <deal.II/fe/fe.h>
#include <deal.II/fe/fe_q.h>
#include <deal.II/fe/fe_system.h>
// tools that allow to discribes the mapping of the deformation on the finite element probleme
#include <deal.II/fe/mapping_q_eulerian.h>
#include <deal.II/fe/mapping_fe_field.h>

#include <deal.II/dofs/dof_tools.h>
#include <deal.II/base/parsed_function.h>
#include <deal.II/numerics/data_out.h>
#include <deal.II/numerics/vector_tools.h>
#include <deal.II/numerics/matrix_tools.h>


// tool to allow user to do cumputation on the non matching grid of the lagrange probleme and the lagrange multiplier probleme

#include <deal.II/non_matching/coupling.h>
#include <deal.II/lac/affine_constraints.h>

// other stuff as usual

#include <deal.II/lac/sparse_matrix.h>
#include <deal.II/lac/vector.h>
#include <deal.II/lac/sparse_direct.h>
#include <deal.II/lac/solver_cg.h>
#include <deal.II/lac/precondition.h>
#include <deal.II/lac/linear_operator.h>
#include <deal.II/lac/linear_operator_tools.h>
#include <iostream>
#include <fstream>


// make it possible to directly call dealII function



//define class that would be call during the finite elements analysis
namespace Mystep60 {
    using namespace dealii;
    template<int dim, int spacedim = dim>
    class DistributedLagrangeProblem {
        //Bonne pratique de limite les fonctions de types public et de regrouper le plus
        // de fonctions possible dans les fonctions privés.
    public:
        // define parameters that the code used at differents part of the programme.
        class Parameters : public ParameterAcceptor {
            //gonna recive all other parameter difined
        public:
            Parameters();

            // define the number of time that the code is gonna refine the first mesh
            unsigned int initial_refinement;
            // define the number of refinement that is applied on the part of the base mesh and the sub domain where the condtions are imposed
            unsigned int delta_refinement;
            // number of refinement of the grid that make the subdomaine
            unsigned int initial_embedded_grid_refinement;
            // we are working on a unit square for this exemple so we need to define which boundary as dirichlet =0
            std::list<types::boundary_id> homogeneous_dirichlet_ids{0, 1, 2, 3};
            // finite element degree on the ebedded domain
            unsigned int embedded_fe_deg = 1;
            // finite  element degree of the general space
            unsigned int domain_fe_deg = 1;
            // Deg of the space that is used to discribe the deformation of the embedded domain
            unsigned int deformation_fe_deg = 1;
            //order of the quadrature formula
            unsigned int coupling_quadrature_order = 3;

            // const bool to define which intepretation is made from the deformation function ( displacement or delta)
            bool use_displacement = false;

            // level of verbosity  for  output data ( ????) present in the exemle code not sure what is it doing
            unsigned int verbosity_lvl = 10;

            // flag is the probleme is initialized or not
            bool initialized = false;

        };

        DistributedLagrangeProblem(const Parameters &parameters);

        void run();

    private:
        // the obeject where the parameters are stored
        const Parameters &parameters;
        // dfine the mesh of the sub domain and the mesh of the domaine


        void setup_grid();

        void setup_matrix();

        void setup_matrix_sub();

        // define the matrix that make the coupling  of the two domain
        void coulpling_system();

        // creat the big coupled systeme matrix
        void define_probleme();


        void solve();

        void output();

        //define global variables of the domain

        // std:: unique_ptr is there to permite overload of variable not sure ?????????????????????????????????????????????????????????????
        std::unique_ptr<Triangulation<spacedim>> mesh;
        std::unique_ptr<GridTools::Cache<spacedim>> mesh_tools;
        std::unique_ptr<FE_Q<spacedim>> fe;
        std::unique_ptr<DoFHandler<spacedim>> dof_handler;


        //define global variable of the embedded domain
        std::unique_ptr<Triangulation<dim>> mesh_sub;
        std::unique_ptr<FE_Q<dim>> fe_sub;
        std::unique_ptr<DoFHandler<dim>> dof_handler_sub;

        // elements needed to do the deformation of the subdomain

        std::unique_ptr<FiniteElement<dim, spacedim>> configuration_FE;
        std::unique_ptr<DoFHandler<dim, spacedim>> configuration_dof_handler;
        Vector<double> configuration;


        // use the Function Parsed Function to replace the communl;y user defined function for the right hand side of the equat
        ParameterAcceptorProxy<Functions::ParsedFunction<spacedim>> configuration_function;
        std::unique_ptr<Mapping<dim, spacedim>> sub_domain_mapping;

        ParameterAcceptorProxy<Functions::ParsedFunction<spacedim>> sub_domain_value_function;

        // do the same whit REduction class let specificy solver control criteria
        ParameterAcceptorProxy<ReductionControl> schur_solver_control;


        // sparsity patterne need during the resolution
        SparsityPattern stiffness_sparsity;
        SparsityPattern coupling_sparsity;
        SparseMatrix<double> stiffnes_matrix;
        SparseMatrix<double> coupling_matrix;

        // make possible to have hanging not and pass boundary condition on it
        AffineConstraints<double> constraints;

        // vector used in the evaluation of the function
        Vector<double> solution;
        Vector<double> rhs;
        Vector<double> lambda;
        Vector<double> sub_domain_rhs;
        Vector<double> sub_domain_value;

        // provide stats of the resolution
        TimerOutput monitor;

    };

// define the parameter file
    template<int dim, int spacedim>
    DistributedLagrangeProblem<dim, spacedim>::Parameters::Parameters():
            ParameterAcceptor("/Distributed Lagrange<" + Utilities::int_to_string(dim) + "," +
                              Utilities::int_to_string(spacedim) + ">/") {

        add_parameter("Initial embedding space refinement", initial_refinement);
        add_parameter("Initial embedded space refinement",
                      initial_embedded_grid_refinement);
        add_parameter("Local refinements steps near embedded domain",
                      delta_refinement);
        add_parameter("Homogeneous Dirichlet boundary ids",
                      homogeneous_dirichlet_ids);
        add_parameter("Use displacement in embedded interface", use_displacement);
        add_parameter("Embedding space finite element degree",
                      domain_fe_deg);
        add_parameter("Embedded space finite element degree",
                      embedded_fe_deg);
        add_parameter("Embedded configuration finite element degree",
                      deformation_fe_deg);
        add_parameter("Coupling quadrature order", coupling_quadrature_order);
        add_parameter("Verbosity level", verbosity_lvl);


        parse_parameters_call_back.connect([&]() -> void { initialized = true; });
    }

    // constructor operation of the parameters and function
    template<int dim, int spacedim>
    DistributedLagrangeProblem<dim, spacedim>::DistributedLagrangeProblem(const Parameters &parameters)
            : parameters(parameters), configuration_function("Embedded configuration", spacedim),
              sub_domain_value_function("embedded value"), schur_solver_control("Schur solver control"),
              monitor(std::cout, TimerOutput::summary, TimerOutput::cpu_and_wall_times) {
        //default value for the parameter Acceptor class created from parameter acceptor proxy

        // define the function and value of the expression of the sub domain
        configuration_function.declare_parameters_call_back.connect([]() -> void {
            ParameterAcceptor::prm.set("Function constants", "R=0.3, Cx=0.4 Cy=0.4");
            ParameterAcceptor::prm.set("Function expression", "R*cos(2*pi*x)+Cx; R*sin(2*pi*x)+Cy");
        });
        // Define the sub domain value function to a csontant
        sub_domain_value_function.declare_parameters_call_back(
                []() -> void { ParameterAcceptor::prm.set("Function expression", "1"); });
        // define parameters of the solver

        schur_solver_control.declare_parameters_call_back([]() -> void {
            ParameterAcceptor::prm.set("Max steps", "1000");
            ParameterAcceptor::prm.set("Reduction", "1.e-12");
            ParameterAcceptor::prm.set("Tolerance", "1.e-12");
        });

    }

// setting up the mesh for the system
    template<int dim, int spacedim>
    void DistributedLagrangeProblem<dim, spacedim>::setup_grid() {
        //output the time of the
        TimerOutput::Scope timer_section(monitor, "setup grids and dofs");
        // generate basic mesh
        mesh = std_cxx14::make_unique<Triangulation<spacedim>>();
        GridGenerator::hyper_cube(*mesh, 0, 1, true);
        // refine it according to the parameters
        mesh->refine_global(parameters.initial_refinement);
        mesh_tools = std_cxx14::make_unique<GridTools::Cache<spacedim, spacedim>>(*mesh);
        // generate mesh of subdomain
        mesh_sub = std_cxx14::make_unique<Triangulation<dim>>();
        GridGenerator::hyper_cube(*mesh_sub);
        mesh_sub->refine_global(parameters.initial_embedded_grid_refinement);

        // generate the finite element sub domain information
        configuration_FE = std_cxx14::make_unique<FESystem<dim, spacedim>>(
                FE_Q<dim, spacedim>(parameters.embedded_fe_deg), spacedim);
        configuration_dof_handler = std_cxx14::make_unique<FESystem<dim, spacedim>>(*mesh_sub);
        configuration_dof_handler->distribute_dofs(*configuration_FE);
        configuration.reinit(configuration_dof_handler->n_dofs());

        // interpolate the configuration and deformation of the domain
        VectorTools::interpolate(*configuration_dof_handler, configuration_function, configuration);

        //mapping the deformation of the sub domain to the subdomain
        if (parameters.use_displacement == true)
            sub_domain_mapping = std_cxx14::make_unique<MappingQEulerian<dim, Vector<double>, spacedim>>(
                    parameters.deformation_fe_deg, *configuration_dof_handler, configuration
            );
        else
            sub_domain_mapping = std_cxx14::make_unique<MappingFEField<dim, spacedim, Vector<double>, DoFHandler<
                    dim, spacedim>>>(*configuration_dof_handler, configuration, configuration);

        // set it up on the sub matrix domain
        setup_matrix_sub();


        //define the support point of the sub domain so we can refine arrond it in a later operation
        std::vector<Point<spacedim>> support_point(*dof_handler_sub->n_dofs());
        if (parameters.delta_refinement != 0)
            DoFTools::map_dofs_to_support_points(*sub_domain_mapping, *dof_handler_sub, support_point);

        // set flag for refinement arrond the points that support the sub domain and there neigboring cell
        for (unsigned int i = 0; i < parameters.delta_refinement; i++) {
            const auto point_locations = GridTools::compute_point_locations(*mesh_tools, support_point);
            const auto &cells = std::get<0>(point_locations);
            for (auto &cell : cells) {
                cell->set_refine_flag();
                for (unsigned int face_no = 0; face_no < GeometryInfo<spacedim>::faces_per_cell; ++face_no)
                    if (!cell->at_boundary(face_no)) {
                        auto neighbor = cell->neighbor(face_no);
                        neighbor->set_refine_flag();
                    }

            }
            mesh->excute_coarsening_and_refinement();
        }

        // to have proper results we need the sub domain grid to be in general smaller then the domain grid so in most cases the cells int the sub domaine dont span on more then 2 cell in the general domain
        // give a error if this is not the case

        const double embedded_space_maximal_diameter =
                GridTools::maximal_cell_diameter(*mesh_sub, *sub_domain_mapping);
        double embedding_space_minimal_diameter =
                GridTools::minimal_cell_diameter(*mesh);
        deallog << "Embedding minimal diameter: "
                << embedding_space_minimal_diameter
                << ", embedded maximal diameter: "
                << embedded_space_maximal_diameter << ", ratio: "
                << embedded_space_maximal_diameter /
                   embedding_space_minimal_diameter
                << std::endl;
        AssertThrow(embedded_space_maximal_diameter <
                    embedding_space_minimal_diameter,
                    ExcMessage(
                            "The embedding grid is too refined (or the embedded grid "
                            "is too coarse). Adjust the parameters so that the minimal "
                            "grid size of the embedding grid is larger "
                            "than the maximal grid size of the embedded grid."));

        //setup the global mesh from there
        setup_matrix();
    }


    template<int dim, int spacedim>
    void DistributedLagrangeProblem<dim, spacedim>::setup_matrix() {
        //standards stuff for fe and dofs
        dof_handler = std_cxx14::make_unique<DoFHandler<spacedim>>(*mesh);
        fe = std_cxx14::make_unique<FE_Q<spacedim>>(parameters.domain_fe_deg);
        dof_handler->distribute_dofs(*fe);
        // generate constraint element for the nodes and the boundary condition
        DoFTools::make_hanging_node_constraints(*mesh, constraints);
        for (auto id : parameters.homogeneous_dirichlet_ids) {
            VectorTools::interpolate_boundary_values(dof_handler, id, Functions::ZeroFunction<spacedim>(), constraints);
        }
        constraints.close();

        //define the dynamic sparcdity pattern for the domain

        DynamicSparsityPattern dsp(dof_handler.n_dofs(), dof_handler.n_dofs());
        DoFTools::make_sparsity_pattern(dof_handler, dsp, constraints);
        stiffness_sparsity.copy_from(dsp);
        stiffnes_matrix.reinit(dof_handler.n_dofs);
        rhs.reinit(dof_handler.n_dofs);
        deallog << "embedding Dofs: " << dof_handler.n_dofs << std::endl;

    }

    template<int dim, int spacedim>
    void DistributedLagrangeProblem<dim, spacedim>::setup_matrix_sub() {
        //generate usual stuff for the sub domain
        dof_handler_sub = std_cxx14::make_unique<DoFHandler<dim>> * (*mesh_sub);
        fe_sub = std_cxx14::make_unique<FE_Q<dim>> * (parameters.embedded_fe_deg);
        dof_handler_sub.distributes_dofs(*fe_sub);

        // define the value of the sub domaine

        lambda.reinit(dof_handler_sub.n_dofs());
        sub_domain_rhs.reinit(dof_handler_sub.n_dofs());
        sub_domain_value.reinit(dof_handler_sub.n_dofs());

        deallog << "Embedded dofs:" << dof_handler_sub.n_dofs() << std::endl;


    }

    template<int dim, int spacedim>
    void DistributedLagrangeProblem<dim, spacedim>::coulpling_system() {
        // define the assembling og the two subdomain
        TimerOutput::Scope timer_section(monitor, "Setup coupling");

        QGauss<dim> quad(parameters.coupling_quadrature_order);
        DynamicSparsityPattern dsp(dof_handler.n_dofs(), dof_handler_sub.n_dofs());

        //match the two grid value  whit the systeme containe in a single object
        NonMatching::create_coupling_sparsity_pattern(*mesh_tools, *dof_handler, *dof_handler_sub, quad, dsp,
                                                      AffineConstraints < double > (), ComponentMask(), ComponentMask(),
                                                      *sub_domain_mapping);
        //
        coupling_sparsity.copy_from(dsp);
        coupling_matrix.reinit(coupling_sparsity);
    }


    template<int dim, int spacedim>
    void DistributedLagrangeProblem<dim, spacedim>::define_probleme() {
        {//Assemble the matrix and the right hand side whit fancy function contrary to the usual loop
            TimerOutput::Scope timer_section(monitor, "Assemble System");
            MatrixTools::create_laplace_matrix(*dof_handler, QGauss<spacedim>(2 * fe.degree() + 1), stiffnes_matrix,
                                               static_cast<const Function<spacedim> *>(nullptr), constraints);
            VectorTools::create_right_hand_side(*sub_domain_mapping, *dof_handler_sub,
                                                QGauss<dim>(2 * fe_sub.degree() + 1), sub_domain_value, sub_domain_rhs);


        }
        {// Assemble coupling systeme and the G function whit fancy function because it allow to group all mapping of the two mesh in one object
            TimerOutput::Scope timer_section(monitor, "Assemble Coupling");
            QGauss<dim> quad(parameters.coupling_quadrature_order);
            NonMatching::create_coupling_mass_matrix(*mesh_tools, *dof_handler, *dof_handler_sub, quad, coupling_matrix,
                                                     AffineConstraints < double > (), ComponentMask(), ComponentMask(),
                                                     *sub_domain_mapping);
            VectorTools::interpolate(*sub_domain_mapping, *dof_handler_sub, sub_domain_value_function,
                                     sub_domain_value);

        }
    }


    template<int dim, int spacedim>
    void DistributedLagrangeProblem<dim, spacedim>::solve() {
        //solve the probleme
        TimerOutput::Scope timer_section(monitor, "Solve");
        // developpe the inverse of the the stiffness matrix

        SparseDirectUMFPACK K_inv_umfpack;
        K_inv_umfpack.initialize(stiffnes_matrix);
        auto K = linear_operator(stiffnes_matrix);
        auto Ct = linear_operator(coupling_matrix);
        auto C = transpose_operator(Ct);
        auto K_inv = linear_operator(K, K_inv_umfpack);

        //Schur Complement method
        auto S = C * K_inv * Ct;
        SolverCG<Vector<double>> solver_cg(schur_solver_control);
        auto S_inv = inverse_operator(S, solver_cg, PreconditionIdentity());
        lambda = S_inv * sub_domain_rhs;
        solution = K_inv * Ct * lambda;
        constraints.distribute(solution);
    }

    template<int dim, int spacedim>
    void DistributedLagrangeProblem<dim, spacedim>::output() {

        TimerOutput::Scope timer_section(monitor, "Output results");
        DataOut<spacedim> embedding_out;

        std::ofstream embedding_out_file("embedding.vtu");
// ouput domain results
        embedding_out.attach_dof_handler(*dof_handler);
        embedding_out.add_data_vector(solution, "solution");
        embedding_out.build_patches(parameters.embedded_fe_deg);
        embedding_out.write_vtu(embedding_out_file);

        // output subdomain results
        DataOut<dim, DoFHandler<dim, spacedim>> embedded_out;
        std::ofstream embedded_out_file("embedded.vtu");
        embedded_out.attach_dof_handler(*dof_handler_sub);
        embedded_out.add_data_vector(lambda, "lambda");
        embedded_out.add_data_vector(sub_domain_value, "g");
        embedded_out.build_patches(*sub_domain_mapping,
                                   parameters.domain_fe_deg);
        embedded_out.write_vtu(embedded_out_file);

    }


    template<int dim, int spacedim>
    void DistributedLagrangeProblem<dim, spacedim>::run() {
        //control the printing operation
        AssertThrow(parameters.initialized, ExcNotInitialized());
        deallog.depth_console(parameters.verbosity_lvl);

        setup_grid();
        coulpling_system();
        define_probleme();
        solve();
        output();


    }
}

int main (int argc, char **argv) {
    try {
        using namespace dealii;
        using namespace Mystep60;
        const unsigned int dim = 1, spacedim = 2;
        DistributedLagrangeProblem<dim, spacedim>::Parameters parameters;
        DistributedLagrangeProblem<dim, spacedim> problem(parameters);
        std::string parameter_file;
        if (argc > 1)
            parameter_file = argv[1];
        else
            parameter_file = "parameters.prm";
        ParameterAcceptor::initialize(parameter_file, "used_parameters.prm");
        problem.run();
    }
    catch (std::exception &exc)
    {
        std::cerr << std::endl
                  << std::endl
                  << "----------------------------------------------------"
                  << std::endl;
        std::cerr << "Exception on processing: " << std::endl
                  << exc.what() << std::endl
                  << "Aborting!" << std::endl
                  << "----------------------------------------------------"
                  << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << std::endl
                  << std::endl
                  << "----------------------------------------------------"
                  << std::endl;
        std::cerr << "Unknown exception!" << std::endl
                  << "Aborting!" << std::endl
                  << "----------------------------------------------------"
                  << std::endl;
        return 1;
    }
    return 0;

}