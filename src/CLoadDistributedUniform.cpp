/* +---------------------------------------------------------------------------+
   |              OpenBeam - C++ Finite Element Analysis library               |
   |                                                                           |
   |   Copyright (C) 2010-2013  Jose Luis Blanco Claraco                       |
   |                              University of Malaga                         |
   |                                                                           |
   | OpenBeam is free software: you can redistribute it and/or modify          |
   |     it under the terms of the GNU General Public License as published by  |
   |     the Free Software Foundation, either version 3 of the License, or     |
   |     (at your option) any later version.                                   |
   |                                                                           |
   | OpenBeam is distributed in the hope that it will be useful,               |
   |     but WITHOUT ANY WARRANTY; without even the implied warranty of        |
   |     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         |
   |     GNU General Public License for more details.                          |
   |                                                                           |
   |     You should have received a copy of the GNU General Public License     |
   |     along with OpenBeam.  If not, see <http://www.gnu.org/licenses/>.     |
   |                                                                           |
   +---------------------------------------------------------------------------+ */

#include <openbeam/loads.h>
#include <openbeam/elements.h>
#include <openbeam/CFiniteElementProblem.h>
#include <openbeam/CStructureProblem.h>

using namespace std;
using namespace openbeam;

void CLoadDistributedUniform::computeStressAndEquivalentLoads(
	const CElement                * el,
	TElementStress  & stress,
	std::vector<array6 >  & loads
	)
{
	// make sure director vector is unitary:
	OBASSERT_DEBUG( std::abs(1 - (dir[0]*dir[0]+dir[1]*dir[1]+dir[2]*dir[2]))<1e-6 )

	const TRotationTrans3D & node0 = el->getParent()->getNodePose(el->conected_nodes_ids[0]);
    const TRotationTrans3D & node1 = el->getParent()->getNodePose(el->conected_nodes_ids[1]);

	num_t Ax = node1.t.coords[0]-node0.t.coords[0];
	num_t Ay = node1.t.coords[1]-node0.t.coords[1];
	num_t Az = node1.t.coords[2]-node0.t.coords[2];

    const num_t L2 = square(Ax)+square(Ay)+square(Az);
	const num_t L = std::sqrt(L2);
	OBASSERT(L>0)
	const num_t _1_L = 1 / L;

	// Normalize direction vector:
	Ax*=_1_L;
	Ay*=_1_L;
	Az*=_1_L;

    OB_TODO("Stress: general 3D case")
	// TODO: Make this real for 3D: reaction should be perp.
	//  to the beam in the direction defined by the plane "beam<->load dir".

	// Cross product: A x dir
	const num_t cross_prod[3] = {
		Ay*dir[2]-Az*dir[1],
		-Ax*dir[2]+Az*dir[0],
		Ax*dir[1]-Ay*dir[0] };


	// Split "q" into perpendicular & tangent load densities:
	const num_t proy_long = (Ax*dir[0]+Ay*dir[1]+Az*dir[2]);
	const num_t proy_perp = std::sqrt( 1 - proy_long*proy_long ) * (cross_prod[2]<0 ? +1.:-1.);

	const num_t q_t = q * proy_long;
	const num_t q_n = q * proy_perp;


	// Stress -------------------------------------------

	// Stress components, in the element local coordinates:
	Eigen::Matrix<num_t,3,1>  f1,f2;
	f1.setZero();
	f2.setZero();

	num_t m1,m2;

	if ( dynamic_cast<const CElementBeam_2D_AA *>(el)!=NULL )
	{
		const CElementBeam_2D_AA *e = dynamic_cast<const CElementBeam_2D_AA*>(el);

		f1[0] =-q_t*L*0.5;
		f1[1] = q_n*L*0.5;
		m1  = 0;
		f2[0] =-q_t*L*0.5;
		f2[1] = q_n*L*0.5;
		m2  = 0;
	}
	else if ( dynamic_cast<const CElementBeam_2D_RR *>(el)!=NULL )
	{
		const CElementBeam_2D_RR *e = dynamic_cast<const CElementBeam_2D_RR*>(el);

		f1[0] =-q_t*L*0.5;
		f1[1] = q_n*L*0.5;
		m1  =+q_n*L*L/12;
		f2[0] =-q_t*L*0.5;
		f2[1] = q_n*L*0.5;
		m2  =-q_n*L*L/12;
	}
	else if ( dynamic_cast<const CElementBeam_2D_RA *>(el)!=NULL )
	{
		const CElementBeam_2D_RA *e = dynamic_cast<const CElementBeam_2D_RA*>(el);

		f1[0] =-q_t*L*0.5;
		f1[1] = q_n*L*(5./8);
		m1  =+q_n*L*L/8;
		f2[0] =-q_t*L*0.5;
		f2[1] = q_n*L*(3./8);
		m2  = 0;
	}
	else if ( dynamic_cast<const CElementBeam_2D_AR *>(el)!=NULL )
	{
		const CElementBeam_2D_AR *e = dynamic_cast<const CElementBeam_2D_AR*>(el);

		f1[0] =-q_t*L*0.5;
		f1[1] = q_n*L*(3./8);
		m1  = 0;
		f2[0] =-q_t*L*0.5;
		f2[1] = q_n*L*(5./8);
		m2  = -q_n*L*L/8;
	}
	else throw std::runtime_error("Unsupported element type for load 'CLoadConstTemperature'");

#if 0
	cout << "L = " << L << " f1: " << f1.transpose() << "<br>" <<endl;
	cout << "f2: " << f2.transpose() << "<br>" <<endl;
#endif

	// Stress:
	stress.resize(2);

	stress[0].N =-f1[0];
	stress[0].Vy=-f1[1];
	stress[0].Mz= -m1;

	stress[1].N = f2[0];
	stress[1].Vy= f2[1];
	stress[1].Mz=  m2;

	// Equivalent load -------------------------------------------
	const TMatrix33 &R = el->getGlobalOrientation().getRot();  // Reference to the 3D rotation matrix (3x3) of the beam

	const Eigen::Matrix<num_t,3,1> F1 = -R * f1;  // "-" since loads are the inverse of reactions/stress.
	const Eigen::Matrix<num_t,3,1> F2 = -R * f2;

	loads.resize(2);

	loads[0][0] = F1[0];
	loads[0][1] = F1[1];
	loads[0][2] = F1[2];
	loads[0][3] = 0;
	loads[0][4] = 0;
	loads[0][5] =-m1;

	loads[1][0] = F2[0];
	loads[1][1] = F2[1];
	loads[1][2] = F2[2];
	loads[1][3] = 0;
	loads[1][4] = 0;
	loads[1][5] =-m2;

}

/** Parse a set of parameters by (casi insensitive) name and set the element values from them. */
void CLoadDistributedUniform::loadParamsFromSet( const TParamSet & params,const TEvaluationContext &eval)
{
	for (TParamSet::const_iterator it=params.begin();it!=params.end();++it)
	{
		if (strCmpI(it->first,"Q"))
			{ eval.parser_evaluate_expression(it->second,this->q); }
		else if (strCmpI(it->first,"DX"))
			{ eval.parser_evaluate_expression(it->second,this->dir[0]); }
		else if (strCmpI(it->first,"DY"))
			{ eval.parser_evaluate_expression(it->second,this->dir[1]); }
		else if (strCmpI(it->first,"DZ"))
			{ eval.parser_evaluate_expression(it->second,this->dir[2]); }
        else { if (eval.warn_msgs) eval.warn_msgs->push_back(format("*Warning* Ignoring unknown parameter %s",it->first.c_str())); }
	}
}

/** Decompose the distributed load as needed into the set of elements in which the original element has been meshed */
void CLoadDistributedUniform::meshLoad(CStructureProblem &meshed_fem,const std::vector<size_t> & meshed_element_idxs, const size_t original_bar_idx, const CStructureProblem & original_fem) const
{
	// Distribute the load into identical distributed loads:
	for (size_t i=0;i<meshed_element_idxs.size();i++)
		meshed_fem.addLoadAtBeam( meshed_element_idxs[i],  new CLoadDistributedUniform(*this));
}

