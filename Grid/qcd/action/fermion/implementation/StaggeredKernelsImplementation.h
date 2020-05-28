/*************************************************************************************

Grid physics library, www.github.com/paboyle/Grid

Source file: ./lib/qcd/action/fermion/WilsonKernels.cc

Copyright (C) 2015

Author: Azusa Yamaguchi, Peter Boyle

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

See the full license in the file "LICENSE" in the top level distribution
directory
*************************************************************************************/
/*  END LEGAL */
#include <Grid/qcd/action/fermion/FermionCore.h>

#pragma once

NAMESPACE_BEGIN(Grid);

#define GENERIC_STENCIL_LEG(U,Dir,skew,multLink)		\
  SE = st.GetEntry(ptype, Dir+skew, sF);			\
  if (SE->_is_local ) {						\
    if (SE->_permute) {						\
      chi_p = &chi;						\
      permute(chi,  in[SE->_offset], ptype);			\
    } else {							\
      chi_p = &in[SE->_offset];					\
    }								\
  } else {							\
    chi_p = &buf[SE->_offset];					\
  }								\
  multLink(Uchi, U[sU], *chi_p, Dir);			

#define GENERIC_STENCIL_LEG_INT(U,Dir,skew,multLink)		\
  SE = st.GetEntry(ptype, Dir+skew, sF);			\
  if (SE->_is_local ) {						\
    if (SE->_permute) {						\
      chi_p = &chi;						\
      permute(chi,  in[SE->_offset], ptype);			\
    } else {							\
      chi_p = &in[SE->_offset];					\
    }								\
  } else if ( st.same_node[Dir] ) {				\
    chi_p = &buf[SE->_offset];					\
  }								\
  if (SE->_is_local || st.same_node[Dir] ) {			\
    multLink(Uchi, U[sU], *chi_p, Dir);				\
  }

#define GENERIC_STENCIL_LEG_EXT(U,Dir,skew,multLink)		\
  SE = st.GetEntry(ptype, Dir+skew, sF);			\
  if ((!SE->_is_local) && (!st.same_node[Dir]) ) {		\
    nmu++;							\
    chi_p = &buf[SE->_offset];					\
    multLink(Uchi, U[sU], *chi_p, Dir);				\
  }

template <class Impl>
StaggeredKernels<Impl>::StaggeredKernels(const ImplParams &p) : Base(p){};

////////////////////////////////////////////////////////////////////////////////////
// Generic implementation; move to different file?
// Int, Ext, Int+Ext cases for comms overlap
////////////////////////////////////////////////////////////////////////////////////
template <class Impl>
template <int Naik>
void StaggeredKernels<Impl>::DhopSiteGeneric(StencilView &st, 
					     DoubledGaugeFieldView &U, DoubledGaugeFieldView &UUU,
					     SiteSpinor *buf, int sF, int sU, 
					     const FermionFieldView &in, FermionFieldView &out, int dag) 
{
  const SiteSpinor *chi_p;
  SiteSpinor chi;
  SiteSpinor Uchi;
  StencilEntry *SE;
  int ptype;
  int skew;

  //  for(int s=0;s<LLs;s++){
  //
  //    int sF=LLs*sU+s;
  {
    skew = 0;
    GENERIC_STENCIL_LEG(U,Xp,skew,Impl::multLink);
    GENERIC_STENCIL_LEG(U,Yp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(U,Zp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(U,Tp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(U,Xm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(U,Ym,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(U,Zm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(U,Tm,skew,Impl::multLinkAdd);
    if ( Naik ) {
    skew=8;
    GENERIC_STENCIL_LEG(UUU,Xp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(UUU,Yp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(UUU,Zp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(UUU,Tp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(UUU,Xm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(UUU,Ym,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(UUU,Zm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG(UUU,Tm,skew,Impl::multLinkAdd);
    }
    if ( dag ) { 
      Uchi = - Uchi;
    } 
    vstream(out[sF], Uchi);
  }
};

  ///////////////////////////////////////////////////
  // Only contributions from interior of our node
  ///////////////////////////////////////////////////
template <class Impl>
template <int Naik>
void StaggeredKernels<Impl>::DhopSiteGenericInt(StencilView &st, 
						DoubledGaugeFieldView &U, DoubledGaugeFieldView &UUU,
						SiteSpinor *buf, int sF, int sU, 
						const FermionFieldView &in, FermionFieldView &out,int dag) {
  const SiteSpinor *chi_p;
  SiteSpinor chi;
  SiteSpinor Uchi;
  StencilEntry *SE;
  int ptype;
  int skew ;

  //  for(int s=0;s<LLs;s++){
  //    int sF=LLs*sU+s;
  {
    skew = 0;
    Uchi=Zero();
    GENERIC_STENCIL_LEG_INT(U,Xp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(U,Yp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(U,Zp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(U,Tp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(U,Xm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(U,Ym,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(U,Zm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(U,Tm,skew,Impl::multLinkAdd);
    if ( Naik ) {
    skew=8;
    GENERIC_STENCIL_LEG_INT(UUU,Xp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(UUU,Yp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(UUU,Zp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(UUU,Tp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(UUU,Xm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(UUU,Ym,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(UUU,Zm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_INT(UUU,Tm,skew,Impl::multLinkAdd);
    }
    if ( dag ) {
      Uchi = - Uchi;
    }
    vstream(out[sF], Uchi);
  }
};


  ///////////////////////////////////////////////////
  // Only contributions from exterior of our node
  ///////////////////////////////////////////////////
template <class Impl>
template <int Naik>
void StaggeredKernels<Impl>::DhopSiteGenericExt(StencilView &st, 
						DoubledGaugeFieldView &U, DoubledGaugeFieldView &UUU,
						SiteSpinor *buf, int sF, int sU,
						const FermionFieldView &in, FermionFieldView &out,int dag) {
  const SiteSpinor *chi_p;
  //  SiteSpinor chi;
  SiteSpinor Uchi;
  StencilEntry *SE;
  int ptype;
  int nmu=0;
  int skew ;

  //  for(int s=0;s<LLs;s++){
  //    int sF=LLs*sU+s;
  {
    skew = 0;
    Uchi=Zero();
    GENERIC_STENCIL_LEG_EXT(U,Xp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(U,Yp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(U,Zp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(U,Tp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(U,Xm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(U,Ym,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(U,Zm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(U,Tm,skew,Impl::multLinkAdd);
    if ( Naik ) {
    skew=8;
    GENERIC_STENCIL_LEG_EXT(UUU,Xp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(UUU,Yp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(UUU,Zp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(UUU,Tp,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(UUU,Xm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(UUU,Ym,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(UUU,Zm,skew,Impl::multLinkAdd);
    GENERIC_STENCIL_LEG_EXT(UUU,Tm,skew,Impl::multLinkAdd);
    }
    if ( nmu ) { 
      if ( dag ) { 
	out[sF] = out[sF] - Uchi;
      } else { 
	out[sF] = out[sF] + Uchi;
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////////
// Driving / wrapping routine to select right kernel
////////////////////////////////////////////////////////////////////////////////////
template <class Impl>
void StaggeredKernels<Impl>::DhopDirKernel(StencilImpl &st, DoubledGaugeFieldView &U, DoubledGaugeFieldView &UUU, SiteSpinor * buf,
					   int sF, int sU, const FermionFieldView &in, FermionFieldView &out, int dir,int disp)
{
  // Disp should be either +1,-1,+3,-3
  // What about "dag" ?
  // Because we work out pU . dS/dU 
  // U
  assert(0);
}

#define KERNEL_CALLNB(A,improved)					\
  const uint64_t    NN = Nsite*Ls;					\
  accelerator_forNB( ss, NN, Simd::Nsimd(), {				\
      int sF = ss;							\
      int sU = ss/Ls;							\
      ThisKernel:: template A<improved>(st_v,U_v,UUU_v,buf,sF,sU,in_v,out_v,dag); \
    });

#define KERNEL_CALL(A,improved) KERNEL_CALLNB(A,improved); accelerator_barrier(); 

#define ASM_CALL(A)							\
  const uint64_t    NN = Nsite*Ls;					\
  thread_for( ss, NN, {							\
      int sF = ss;							\
      int sU = ss/Ls;							\
      ThisKernel::A(st_v,U_v,UUU_v,buf,sF,sU,in_v,out_v,dag);		\
  });

template <class Impl>
void StaggeredKernels<Impl>::DhopImproved(StencilImpl &st, LebesgueOrder &lo, 
					  DoubledGaugeField &U, DoubledGaugeField &UUU, 
					  const FermionField &in, FermionField &out, int dag, int interior,int exterior)
{
  GridBase *FGrid=in.Grid();  
  GridBase *UGrid=U.Grid();  
  typedef StaggeredKernels<Impl> ThisKernel;
  auto UUU_v = UUU.View(AcceleratorRead);
  auto U_v   =   U.View(AcceleratorRead);
  auto in_v  =  in.View(AcceleratorRead);
  auto out_v = out.View(AcceleratorWrite);
  auto st_v  =  st.View(AcceleratorRead);
  SiteSpinor * buf = st.CommBuf();
    
  int Ls=1;
  if(FGrid->Nd()==UGrid->Nd()+1){
    Ls    = FGrid->_rdimensions[0];
  }
  int Nsite = UGrid->oSites();

  if( interior && exterior ) { 
    if (Opt == OptGeneric    ) { KERNEL_CALL(DhopSiteGeneric,1); return;}
#ifndef GRID_CUDA
    if (Opt == OptHandUnroll ) { KERNEL_CALL(DhopSiteHand,1);    return;}
    if (Opt == OptInlineAsm  ) {  ASM_CALL(DhopSiteAsm);     return;}
#endif
  } else if( interior ) {
    if (Opt == OptGeneric    ) { KERNEL_CALL(DhopSiteGenericInt,1); return;}
#ifndef GRID_CUDA
    if (Opt == OptHandUnroll ) { KERNEL_CALL(DhopSiteHandInt,1);    return;}
#endif
  } else if( exterior ) { 
    if (Opt == OptGeneric    ) { KERNEL_CALL(DhopSiteGenericExt,1); return;}
#ifndef GRID_CUDA
    if (Opt == OptHandUnroll ) { KERNEL_CALL(DhopSiteHandExt,1);    return;}
#endif
  }
  assert(0 && " Kernel optimisation case not covered ");
}
template <class Impl>
void StaggeredKernels<Impl>::DhopNaive(StencilImpl &st, LebesgueOrder &lo, 
				       DoubledGaugeField &U,
				       const FermionField &in, FermionField &out, int dag, int interior,int exterior)
{
  GridBase *FGrid=in.Grid();  
  GridBase *UGrid=U.Grid();  
  typedef StaggeredKernels<Impl> ThisKernel;
  auto UUU_v=   U.View(AcceleratorRead);
  auto U_v   =   U.View(AcceleratorRead);
  auto in_v  =  in.View(AcceleratorRead);
  auto out_v = out.View(AcceleratorWrite);
  auto st_v  =  st.View(AcceleratorRead);
  SiteSpinor * buf = st.CommBuf();

  int Ls=1;
  if(FGrid->Nd()==UGrid->Nd()+1){
    Ls    = FGrid->_rdimensions[0];
  }
  int Nsite = UGrid->oSites();
  
  if( interior && exterior ) { 
    if (Opt == OptGeneric    ) { KERNEL_CALL(DhopSiteGeneric,0); return;}
#ifndef GRID_CUDA
    if (Opt == OptHandUnroll ) { KERNEL_CALL(DhopSiteHand,0);    return;}
#endif
  } else if( interior ) {
    if (Opt == OptGeneric    ) { KERNEL_CALL(DhopSiteGenericInt,0); return;}
#ifndef GRID_CUDA
    if (Opt == OptHandUnroll ) { KERNEL_CALL(DhopSiteHandInt,0);    return;}
#endif
  } else if( exterior ) { 
    if (Opt == OptGeneric    ) { KERNEL_CALL(DhopSiteGenericExt,0); return;}
#ifndef GRID_CUDA
    if (Opt == OptHandUnroll ) { KERNEL_CALL(DhopSiteHandExt,0);    return;}
#endif
  }
}


#undef KERNEL_CALLNB
#undef KERNEL_CALL
#undef ASM_CALL

NAMESPACE_END(Grid);


