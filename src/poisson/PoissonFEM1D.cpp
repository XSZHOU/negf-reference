/*
Copyright (c) 2010 Sebastian Steiger, Integrated Systems Laboratory, ETH Zurich.
Comments, suggestions, criticism or bug reports are welcome: steiger@purdue.edu. 

This file is part of ANGEL, a simulator for LEDs based on the NEGF formalism.
The software is distributed under the Lesser GNU General Public License (LGPL).
ANGEL is free software: you can redistribute it and/or modify it under the terms 
of the Lesser GNU General Public License v3 or later. ANGEL is distributed
without any warranty; without even the implied warranty of merchantability or 
fitness for a particular purpose. See also <http://www.gnu.org/licenses/>.
*/
#include "PoissonFEM1D.h"
using namespace negf;

PoissonFEM1D::PoissonFEM1D(const Geometry * grid_, const vector<double> & static_rhs_):
    grid(grid_),
    Nvert(grid->get_num_vertices()),
    piezo_decreaser(1.0),
    neumann_field(0.0)
{STACK_TRACE(
    // number_of_variables, its_type and grid have already been set in the Poisson() constructor:
    NEGF_ASSERT(grid!=0, "null pointer encountered.");
    uint num_elems = grid->get_num_elements();

    // ----------------------------------------------------------------------------------------------------------
    // The FEM-discretized Poisson matrix reads as follows:
    // A*phi = M*rho
    // here phi is the potential, rho is the space charge, A is the stiffness matrix and M is the mass matrix
    // ----------------------------------------------------------------------------------------------------------

    // set up arrays storing:
    // - the number of the element to the left  side for each vertex
    // - the number of the element to the right side for each vertex
    // - the static dielectric constant for each element
    // - the length for each element
    vector<int>  left_element(Nvert, -1);
    vector<int> right_element(Nvert, -1);
    for (uint ii=0; ii<Nvert; ii++) {
        Vertex * v = grid->get_vertex(ii);
        const vector<Element *> & elems_near_ii = grid->get_elems_near(v);
        NEGF_ASSERT(elems_near_ii.size()==1 || elems_near_ii.size()==2, "expected 1 or 2 adjacent elements.");
        for (uint jj=0; jj<elems_near_ii.size(); jj++) {
            Edge * e = elems_near_ii[jj]->get_edge(0);
            Vertex * v2 = (e->get_lower_vertex()==v) ? e->get_upper_vertex() : e->get_lower_vertex();
            if (v2->get_coordinate(0) > v->get_coordinate(0)) {
                right_element[ii] = elems_near_ii[jj]->get_index_global();
            } else {
                left_element[ii] = elems_near_ii[jj]->get_index_global();
            }
        }
    }
    vector<double>    eps(num_elems, 0.0);
    vector<double> length(num_elems, 0.0);
    const double eps0 = constants::convert_from_SI(units::dielectric, constants::SIeps0);
    for (uint ii=0; ii<num_elems; ii++) {
           eps[ii]  =  eps0 * grid->get_element(ii)->get_region()->get_material()->get("static_dielectric_constant");
        length[ii]  = grid->get_element(ii)->get_edge(0)->get_length();
    }

    // -----------------------------------------------------------------------------
    // set up prow, icol (sparsity pattern) - same for stiffness and mass matrices
    // we assume that the 1D grid looks like this: 0 -- 1 -- 2 -- ... -- Nvert-1
    // -----------------------------------------------------------------------------
    this->prow.resize(Nvert+1, 0);
    uint count=0;
    for (uint ii=0; ii<Nvert; ii++) {
        this->prow[ii] = count;
        if (ii!=0) {
            this->icol.push_back(ii-1);
            count++;
        }

        this->icol.push_back(ii);
        count++;

        if (ii!=Nvert-1) {
            this->icol.push_back(ii+1);
            count++;
        }
    }
    uint num_nonzeros = count;
    prow[Nvert] = num_nonzeros;
    logmsg->emit(LOG_INFO,"There are %d nonzeros in the stiffness and mass matrices", num_nonzeros);

    // ----------------------------------------------
    // set up mass matrix
    // ----------------------------------------------
    this->mass_matrix.resize(num_nonzeros, 0.0);
    count = 0;
    for (uint ii=0; ii<Nvert; ii++) {
        if (ii!=0) {
            this->mass_matrix[count] = 1.0/6.0 * length[left_element[ii]];
            count++;
        }

        this->mass_matrix[count] = 1.0/3.0 * (
                                        ((ii!=      0) ? length[ left_element[ii]] : 0.0)
                                    +   ((ii!=Nvert-1) ? length[right_element[ii]] : 0.0) );
        count++;

        if (ii!=Nvert-1) {
            this->mass_matrix[count] = 1.0/6.0 * length[right_element[ii]];
            count++;
        }
    }

    // ----------------------------------------------
    // set up stiffness matrix
    // ----------------------------------------------
    this->stiffness_matrix.resize(num_nonzeros, 0.0);
    count = 0;
    for (uint ii=0; ii<Nvert; ii++) {
        if (ii!=0) {
            this->stiffness_matrix[count] = - eps[left_element[ii]] / length[left_element[ii]];
            count++;
        }

        this->stiffness_matrix[count] = + ((ii!=      0) ? eps[ left_element[ii]] / length[ left_element[ii]] : 0.0)
                                        + ((ii!=Nvert-1) ? eps[right_element[ii]] / length[right_element[ii]] : 0.0) ;
        count++;

        if (ii!=Nvert-1) {
            this->stiffness_matrix[count] = - eps[right_element[ii]] / length[right_element[ii]];
            count++;
        }
    }

    // --------------------------------------------
    // assign polarization (static RHS)
    // --------------------------------------------
    if (static_rhs_.size()==grid->get_num_vertices()) {
        this->static_rhs = static_rhs_;
    } else {
        NEGF_ASSERT(static_rhs_.size()==0, "size of handed over static_rhs should be either 0 or num_vertices.");
        this->static_rhs.assign(grid->get_num_vertices(), 0.0);
    }

    ostringstream sout;
    sout << "prow.size() - 1 (" << prow.size()-1 << ") == grid->get_num_vertices() (" << grid->get_num_vertices() << ")";
    NEGF_ASSERT(prow.size()             == grid->get_num_vertices()+1, sout.str().c_str());
    NEGF_ASSERT(prow[grid->get_num_vertices()] == (signed)stiffness_matrix.size(), "prow[grid_->get_num_vertices()] == stiffness_matrix.size()");
    NEGF_ASSERT(stiffness_matrix.size() == mass_matrix.size(), "stiffness_matrix.size() == mass_matrix.size()");
    NEGF_ASSERT(icol.size()             == stiffness_matrix.size(), "icol.size() == stiffness_matrix.size()");
    NEGF_ASSERT(static_rhs.size()       == (uint)grid->get_num_vertices(), "static_rhs.size() == grid->get_num_vertices()");

);}


void PoissonFEM1D::set_piezo_decreaser(double new_decreaser)
{STACK_TRACE(
    NEGF_ASSERT(new_decreaser >= 0.0 && new_decreaser < 10.0, "invalid decreaser value");
    this->piezo_decreaser = new_decreaser;
);}


/** Return the Newton function (whose roots are searched) evaluated for variable (vertex) with index "line"
*/
double PoissonFEM1D::get_newton_function(uint line) const
{STACK_TRACE(
    if (line==0) {
        cout << "*** PoissonFEM1D*** \n elstat_pot = ";
        for (uint ii=0; ii<grid->get_num_vertices(); ii++) cout << (*elstat_pot)[ii] << "  ";
        cout << endl;
        cout << "edens = ";
        for (uint ii=0; ii<grid->get_num_vertices(); ii++) cout << (*edensity)[ii] << "  ";
        cout << endl;
        //cout << "hdens = ";
        //for (uint ii=0; ii<grid->get_num_vertices(); ii++) cout << (*hdensity)[ii] << "  ";
        //cout << endl;
        //cout << "doping = ";
        //for (uint ii=0; ii<grid->get_num_vertices(); ii++) cout << (*doping)[ii] << "  ";
        //cout << endl;
    }

    const Vertex * vertex = grid->get_vertex(line);
    const double ec = constants::convert_from_SI(units::charge, constants::SIec);
    double result = 0.0;

    NEGF_ASSERT(vertex!=0 && epsilon!=0 && edensity!=0 && hdensity!=0 && doping!=0, "Some quantity is missing.");

    // -------------------------------------------------------------------------
    // expression for newton function:
    // interior: - nabla^2 u - rho = 0;
    // boundary:          u - u_bc = 0
    // -------------------------------------------------------------------------

    // -------------------------------------------------
    // check if vertex is at contact
    // -------------------------------------------------
    if (vertex->is_at_contact() &&
        vertex->get_contact()->get_bndcond(quantities::potential) == bndconds::BC_Dirichlet) {
        // vertex->get_contact_vertex_index() returns the index of the vertex in the list of the contact's vertices
        return (*elstat_pot)[line] - vertex->get_contact()->get_bc_value(quantities::potential, vertex->get_contact_vertex_index());
    }

    // NEGF: we also have to do something in case of contacts and Neumann conditions
    if (vertex->is_at_contact() && vertex->get_contact()->get_bndcond(quantities::potential)==bndconds::BC_Neumann)
    {
        const vector<Edge *> & near_edges = grid->get_edges_near(vertex);
        bool a_device_vertex_is_connected = false;
        Vertex * device_interior_vertex = 0;
        for (uint ii=0; ii < near_edges.size(); ii++)
        {
            Vertex * v = (near_edges[ii]->get_lower_vertex()==vertex)
                ? near_edges[ii]->get_upper_vertex() : near_edges[ii]->get_lower_vertex();
            if (!v->is_at_contact()) {
                a_device_vertex_is_connected = true;
                device_interior_vertex = v;
                break;
            }
        }

        if (!a_device_vertex_is_connected)
        {
            // make sure that a "contact-interior" vertex gets the same value as a
            // vertex directly adjacent to the actual device
            const vector<Vertex *> & contact_verts = vertex->get_contact()->get_contact_vertices();
            Vertex * v_adjacent_to_device = 0;
            for (uint ii=0; ii < contact_verts.size(); ii++) {
                const vector<Edge *> & edges_near_ii = grid->get_edges_near(contact_verts[ii]);
                for (uint jj=0; jj < edges_near_ii.size(); jj++) {
                    Vertex * v = (edges_near_ii[jj]->get_lower_vertex()==contact_verts[ii])
                        ? edges_near_ii[jj]->get_upper_vertex() : edges_near_ii[jj]->get_lower_vertex();
                    if (!v->is_at_contact()) {
                        v_adjacent_to_device = contact_verts[ii];
                        break;
                    }
                }
                if (v_adjacent_to_device!=0) break;
            }
            NEGF_ASSERT(v_adjacent_to_device!=0, "no vertex was found which is adjacent to the device interior !?");

            // also in case of nonzero neumann field, we absolutely need flat potenetial WITHIN the contacts
            // because potential is added to hamiltonian which enters boundary self-energies, and potential MUST be flat there!
            return ((*elstat_pot)[line] - (*elstat_pot)[v_adjacent_to_device->get_index_global()]);
        } else {
            // vertex which is at contact but directly connected to the device interior:
            // continue with normal treatment, normal formula

            // NO! same value as device-internal vertex!
            // desired value = value at device interface - distance * E-field
            if (fabs(this->neumann_field) > 1e-10) NEGF_ASSERT(grid->get_dimension()==1, "Neumann field only supported for d=1!");
            double dist = vertex->get_coordinate(0) - device_interior_vertex->get_coordinate(0);
            return ((*elstat_pot)[line] - (*elstat_pot)[device_interior_vertex->get_index_global()] - dist * this->neumann_field);
        }
    }


    // -------------------------------------------------
    // so, its not a contact, but a standard equation,
    // -------------------------------------------------
    for(int kk = prow[line]; kk < prow[line + 1]; kk++) {
        NEGF_ASSERT((signed)stiffness_matrix.size() > kk, "");
        NEGF_ASSERT((signed)mass_matrix.size() > kk, "");

        // assemble the function starting with stiffness matrix contribution
        result += stiffness_matrix[kk] // epsilon grad Ni * grad Nkk
                * (*elstat_pot)[icol[kk]];

        // subtract the mass matrix contributions containing the nodal charge field
        double charge = ec * ((*hdensity)[icol[kk]] - (*edensity)[icol[kk]] + (*doping)[icol[kk]]);
        result -= mass_matrix[kk] * charge;
    }

    // subtract the static rhs
    if (fabs(piezo_decreaser-1.0)>1e-10 && line==30) cout << "piezo_decreaser=" << this->piezo_decreaser << "    ";
    result -= ec * this->piezo_decreaser * static_rhs[line];

    return result;
);}


/** Get the partial derivative of the Newton function w.r.t. the potential
 *
 * @param line which variable
 * @param nonzeros number storing the number of array entries used
 * @param indices array storing the indices of the dependency equation where the derivative is nonzero
 * @param coeffs array storing the corresponding derivatives. coeffs==NULL --> sparsity pattern only
*/
void PoissonFEM1D::get_newton_derivative(uint line, uint & nonzeros, uint indices[], double coeffs[]) const
{STACK_TRACE(
    const Vertex * vertex = grid->get_vertex(line);
    const double ec = constants::convert_from_SI(units::charge, constants::SIec);

    NEGF_ASSERT(vertex!=0 && epsilon!=0 && edensity!=0 && hdensity!=0 && doping!=0, "Some quantity is missing.");

    nonzeros = 0;

    // -------------------------------------------------------------------------------------
    // treatment of Dirichlet contact vertices (Poisson eqn is replaced with explicit value)
    // -------------------------------------------------------------------------------------
    if(vertex->is_at_contact() && vertex->get_contact()->get_bndcond(quantities::potential)==bndconds::BC_Dirichlet) {
        nonzeros   = 1;
        indices[0] = line;
        // return if this is only a structure inquiry
        if(coeffs == NULL) {
            return;
        }
        // equation is u - u_bc
        coeffs[0]  = 1.0;
        return;
    }

    // -------------------------------------------------------------------------------------
    // NEGF: we also have to do something in case of contacts and Neumann conditions
    // -------------------------------------------------------------------------------------
    if (vertex->is_at_contact() && vertex->get_contact()->get_bndcond(quantities::potential)==bndconds::BC_Neumann)
    {
        const vector<Edge *> & near_edges = grid->get_edges_near(vertex);
        bool a_device_vertex_is_connected = false;
        Vertex * device_interior_vertex = 0;
        for (uint ii=0; ii < near_edges.size(); ii++)
        {
            Vertex * v = (near_edges[ii]->get_lower_vertex()==vertex)
                ? near_edges[ii]->get_upper_vertex() : near_edges[ii]->get_lower_vertex();
            if (!v->is_at_contact()) {
                a_device_vertex_is_connected = true;
                device_interior_vertex = v;
                break;
            }
        }

        if (!a_device_vertex_is_connected)
        {
            // make sure that a "contact-interior" vertex gets the same value as a
            // vertex directly adjacent to the actual device
            const vector<Vertex *> & contact_verts = vertex->get_contact()->get_contact_vertices();
            Vertex * v_adjacent_to_device = 0;
            for (uint ii=0; ii < contact_verts.size(); ii++)
            {
                const vector<Edge *> & edges_near_ii = grid->get_edges_near(contact_verts[ii]);
                for (uint jj=0; jj < edges_near_ii.size(); jj++) {
                    Vertex * v = (edges_near_ii[jj]->get_lower_vertex()==contact_verts[ii])
                        ? edges_near_ii[jj]->get_upper_vertex() : edges_near_ii[jj]->get_lower_vertex();
                    if (!v->is_at_contact()) {
                        v_adjacent_to_device = contact_verts[ii];
                        break;
                    }
                }
                if (v_adjacent_to_device!=0) break;
            }
            NEGF_ASSERT(v_adjacent_to_device!=0, "no vertex was found which is adjacent to the device interior !?");

            nonzeros   = 2;
            indices[0] = line;
            indices[1] = v_adjacent_to_device->get_index_global();
            if (coeffs!=NULL) {
                coeffs[0]  = 1.0;
                coeffs[1]  = -1.0;
            }
            return;
        } else {
            // vertex which is at contact but directly connected to the device interior:
            // continue with normal treatment
            // NO!!! same value as device-interior!
            nonzeros   = 2;
            indices[0] = line;
            indices[1] = device_interior_vertex->get_index_global();
            if (coeffs!=NULL) {
                coeffs[0]  = 1.0;
                coeffs[1]  = -1.0;
            }
            return;
        }
    }

    // ----------------------------------------------
    // the pattern does not change for any equation
    // ----------------------------------------------
    nonzeros = prow[line + 1] - prow[line];
    for(unsigned int kk = 0; kk < nonzeros; kk++) {
        indices[kk] = icol[prow[line] + kk];
    }
    // ----------------------------------------------
    // structure queries are fine with that
    // ----------------------------------------------
    if(coeffs == 0) {
        return;
    }

    // ----------------------------------------------
    // direct dependence of Poisson equation on potential
    // given by int eps grad Ni grad Nj
    // ----------------------------------------------
    // copy stiffness matrix entries
    for(unsigned int kk = 0; kk < nonzeros; kk++) {
        coeffs[kk]  = stiffness_matrix[prow[line] + kk];
    }

    // -----------------------------------------------
    // nonlinear dependence via densities (predictor)
    // -----------------------------------------------

    /**
     *  EFn = Ec - e*(phi-phi_const) + kT * F_{+0.5}^-1(n/Nc)   or
     *  EFp = Ev - e*(phi-phi_const) - kT * F_{+0.5}^-1(p/Nv)
     *
     *  dn/dphi ~  ec/kT * Nc * F_{-0.5}(nu_n),
	 *  dp/dphi ~ -ec/kT * Nv * F_{-0.5}(nu_p)
	 *  where
     *  nu_n = (EFn - Ec + e*(phi-phi_const)) / kT,
     *  nu_p = (Ev - e*(phi-phi_const) - EFp) / kT = -(EFp - Ev + e*(phi-phi_const)) / kT
     *
     *  nu_x is calculated from nu_x = F_{+0.5}^{-1}(dens/Ncv)
     *  --> we don't need EFn, EFp, Ec, Ev, phi, phi_const explicitly
	 *
	 * phi_const is introduced because the ContactFermilevel assumes that phi=0 at 
	 * contact 0 regardless of the doping. Luckily, this doesn't influence the implemented formula.
     */

    // Remember: CAN'T make it linear! Neumann!

    for(unsigned int kk = 0; kk < nonzeros; kk++) {

        int ii = icol[prow[line] + kk];
        double nu_n = negf_math::fermihalf_inverse((*edensity)[ii] / (*Nc)[ii]);
        double nu_p = negf_math::fermihalf_inverse((*hdensity)[ii] / (*Nv)[ii]);
        double dn_dphi =  ec / kT * (*Nc)[ii] * negf_math::fermi_int(-0.5, nu_n);
        double dp_dphi = -ec / kT * (*Nv)[ii] * negf_math::fermi_int(-0.5, nu_p);
        double dcharge_dphi = (-ec*dn_dphi + ec*dp_dphi);

        coeffs[kk] += -1.0 * mass_matrix[prow[line] + kk] * dcharge_dphi; // mass_matrix contains int ec Ni Nj
        // +=, NOT = !!!!!!!
    }
    return;
);}


void PoissonFEM1D::set_elstat_pot(vector<double> * potential_) {
    NEGF_ASSERT(potential_!=0 && potential_->size()==Nvert, "wrong potential.");
    this->elstat_pot = potential_;
}

void PoissonFEM1D::set_edensity(vector<double> * edensity_) {
    NEGF_ASSERT(edensity_!=0 && edensity_->size()==Nvert, "wrong edensity.");
    this->edensity = edensity_;

}

void PoissonFEM1D::set_hdensity(vector<double> * hdensity_) {
    NEGF_ASSERT(hdensity_!=0 && hdensity_->size()==Nvert, "wrong hdensity.");
    this->hdensity = hdensity_;
}

void PoissonFEM1D::set_doping(vector<double> * doping_) {
    NEGF_ASSERT(doping_!=0 && doping_->size()==Nvert, "wrong doping.");
    this->doping = doping_;
}

void PoissonFEM1D::set_epsilon(vector<double> * epsilon_) {
    NEGF_ASSERT(epsilon_!=0 && epsilon_->size()==grid->get_num_elements(), "wrong epsilon.");
    this->epsilon = epsilon_;
}

void PoissonFEM1D::set_Nc(vector<double> * Nc_) {
    NEGF_ASSERT(Nc_!=0 && Nc_->size()==Nvert, "wrong Nc.");
    this->Nc = Nc_;
}

void PoissonFEM1D::set_Nv(vector<double> * Nv_) {
    NEGF_ASSERT(Nv_!=0 && Nv_->size()==Nvert, "wrong Nv.");
    this->Nv = Nv_;
}

void PoissonFEM1D::set_kT(double kT_) {
    NEGF_ASSERT(kT_>0.0, "negative temperature!");
    this->kT = kT_;
}
