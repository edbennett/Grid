#ifndef GRID_STENCIL_H
#define GRID_STENCIL_H

//////////////////////////////////////////////////////////////////////////////////////////
// Must not lose sight that goal is to be able to construct really efficient
// gather to a point stencil code. CSHIFT is not the best way, so need
// additional stencil support.
//
// Stencil based code will pre-exchange haloes and use a table lookup for neighbours.
// This will be done with generality to allow easier efficient implementations.
// Overlap of comms and compute could be semi-automated by tabulating off-node connected,
// and 
//
// Lattice <foo> could also allocate haloes which get used for stencil code.
//
// Grid could create a neighbour index table for a given stencil.
//
// Could also implement CovariantCshift, to fuse the loops and enhance performance.
//
//
// General stencil computation:
//
// Generic services
// 0) Prebuild neighbour tables
// 1) Compute sizes of all haloes/comms buffers; allocate them.
//
// 2) Gather all faces, and communicate.
// 3) Loop over result sites, giving nbr index/offnode info for each
// 
// Could take a 
// SpinProjectFaces 
// start comms
// complete comms 
// Reconstruct Umu
//
// Approach.
//
//////////////////////////////////////////////////////////////////////////////////////////

namespace Grid {

  struct CommsRequest { 
    int words;
    int unified_buffer_offset;
    int tag;
    int to_rank;
    int from_rank;
  } ;


///////////////////////////////////////////////////////////////////
// Gather for when there is no need to SIMD split with compression
///////////////////////////////////////////////////////////////////
template<class vobj,class cobj,class compressor> void 
Gather_plane_simple (Lattice<vobj> &rhs,std::vector<cobj,alignedAllocator<cobj> > &buffer,int dimension,int plane,int cbmask,compressor &compress)
{
  int rd = rhs._grid->_rdimensions[dimension];

  if ( !rhs._grid->CheckerBoarded(dimension) ) {

    int so  = plane*rhs._grid->_ostride[dimension]; // base offset for start of plane 
    int o   = 0;                                    // relative offset to base within plane
    int bo  = 0;                                    // offset in buffer

    // Simple block stride gather of SIMD objects
#pragma omp parallel for collapse(2)
    for(int n=0;n<rhs._grid->_slice_nblock[dimension];n++){
      for(int b=0;b<rhs._grid->_slice_block[dimension];b++){
	buffer[bo++]=compress(rhs._odata[so+o+b]);
      }
      o +=rhs._grid->_slice_stride[dimension];
    }

  } else { 

    int so  = plane*rhs._grid->_ostride[dimension]; // base offset for start of plane 
    int o   = 0;                                      // relative offset to base within plane
    int bo  = 0;                                      // offset in buffer

#pragma omp parallel for collapse(2)
    for(int n=0;n<rhs._grid->_slice_nblock[dimension];n++){
      for(int b=0;b<rhs._grid->_slice_block[dimension];b++){

	int ocb=1<<rhs._grid->CheckerBoardFromOindex(o+b);// Could easily be a table lookup
	if ( ocb &cbmask ) {
	  buffer[bo]=compress(rhs._odata[so+o+b]);
	  bo++;
	}

      }
      o +=rhs._grid->_slice_stride[dimension];
    }
  }
}

///////////////////////////////////////////////////////////////////
// Gather for when there *is* need to SIMD split with compression
///////////////////////////////////////////////////////////////////
template<class cobj,class vobj,class compressor> void 
  Gather_plane_extract(Lattice<vobj> &rhs,std::vector<typename cobj::scalar_type *> pointers,int dimension,int plane,int cbmask,compressor &compress)
{
  int rd = rhs._grid->_rdimensions[dimension];

  if ( !rhs._grid->CheckerBoarded(dimension) ) {

    int so  = plane*rhs._grid->_ostride[dimension]; // base offset for start of plane 
    int o   = 0;                                    // relative offset to base within plane
    int bo  = 0;                                    // offset in buffer

    // Simple block stride gather of SIMD objects
#pragma omp parallel for collapse(2)
    for(int n=0;n<rhs._grid->_slice_nblock[dimension];n++){
      for(int b=0;b<rhs._grid->_slice_block[dimension];b++){
	cobj temp;
	temp=compress(rhs._odata[so+o+b]);
	extract(temp,pointers);
      }
      o +=rhs._grid->_slice_stride[dimension];
    }

  } else { 

    int so  = plane*rhs._grid->_ostride[dimension]; // base offset for start of plane 
    int o   = 0;                                      // relative offset to base within plane
    int bo  = 0;                                      // offset in buffer
    
#pragma omp parallel for collapse(2)
    for(int n=0;n<rhs._grid->_slice_nblock[dimension];n++){
      for(int b=0;b<rhs._grid->_slice_block[dimension];b++){

	int ocb=1<<rhs._grid->CheckerBoardFromOindex(o+b);
	if ( ocb & cbmask ) {
	  cobj temp; 
	  temp =compress(rhs._odata[so+o+b]);
	  extract(temp,pointers);
	}

      }
      o +=rhs._grid->_slice_stride[dimension];
    }
  }
}


  class CartesianStencil { // Stencil runs along coordinate axes only; NO diagonal fill in.
  public:

      int                               _checkerboard;
      int                               _npoints; // Move to template param?
      GridBase *                        _grid;
      
      // npoints of these
      std::vector<int>                  _directions;
      std::vector<int>                  _distances;
      std::vector<int>                  _comm_buf_size;
      std::vector<int>                  _permute_type;

      // npoints x Osites() of these
      std::vector<std::vector<int>    > _offsets;
      std::vector<std::vector<int>    > _is_local;
      std::vector<std::vector<int> >    _permute;

      int _unified_buffer_size;
      int _request_count;

      std::vector<CommsRequest>  CommsRequests;

      CartesianStencil(GridBase *grid,
		       int npoints,
		       int checkerboard,
		       const std::vector<int> &directions,
		       const std::vector<int> &distances);


      // Add to tables for various cases;  is this mistaken. only local if 1 proc in dim
      // Can this be avoided with simpler coding of comms?
      void Local     (int point, int dimension,int shift,int cbmask);
      void Comms     (int point, int dimension,int shift,int cbmask);
      void CopyPlane(int point, int dimension,int lplane,int rplane,int cbmask,int permute);
      void ScatterPlane (int point,int dimension,int plane,int cbmask,int offset);

      // Could allow a functional munging of the halo to another type during the comms.
      // this could implement the 16bit/32bit/64bit compression.
      template<class vobj,class cobj, class compressor> void 
	HaloExchange(Lattice<vobj> &source,std::vector<cobj,alignedAllocator<cobj> > &u_comm_buf,compressor &compress)
      {
	// conformable(source._grid,_grid);
	assert(source._grid==_grid);
	if (u_comm_buf.size() != _unified_buffer_size ) u_comm_buf.resize(_unified_buffer_size);
	int u_comm_offset=0;

	// Gather all comms buffers
	for(int point = 0 ; point < _npoints; point++) {

	  compress.Point(point);

	  int dimension    = _directions[point];
	  int displacement = _distances[point];
	  
	  int fd = _grid->_fdimensions[dimension];
	  int rd = _grid->_rdimensions[dimension];


	  // Map to always positive shift modulo global full dimension.
	  int shift = (displacement+fd)%fd;
	  
     	  int checkerboard = _grid->CheckerBoardDestination(source.checkerboard,shift);
	  assert (checkerboard== _checkerboard);

	  // the permute type
	  int simd_layout     = _grid->_simd_layout[dimension];
	  int comm_dim        = _grid->_processors[dimension] >1 ;
	  int splice_dim      = _grid->_simd_layout[dimension]>1 && (comm_dim);

	  // Gather phase
	  int sshift [2];
	  if ( comm_dim ) {
	    sshift[0] = _grid->CheckerBoardShift(_checkerboard,dimension,shift,0);
	    sshift[1] = _grid->CheckerBoardShift(_checkerboard,dimension,shift,1);
	    if ( sshift[0] == sshift[1] ) {
	      if (splice_dim) {
		GatherStartCommsSimd(source,dimension,shift,0x3,u_comm_buf,u_comm_offset,compress);
	      } else { 
		GatherStartComms(source,dimension,shift,0x3,u_comm_buf,u_comm_offset,compress);
	      }
	    } else {
	      if(splice_dim){
		GatherStartCommsSimd(source,dimension,shift,0x1,u_comm_buf,u_comm_offset,compress);// if checkerboard is unfavourable take two passes
		GatherStartCommsSimd(source,dimension,shift,0x2,u_comm_buf,u_comm_offset,compress);// both with block stride loop iteration
	      } else {
		GatherStartComms(source,dimension,shift,0x1,u_comm_buf,u_comm_offset,compress);
		GatherStartComms(source,dimension,shift,0x2,u_comm_buf,u_comm_offset,compress);
	      }
	    }
	  }
	}
      }

      template<class vobj,class cobj, class compressor> 
        void GatherStartComms(Lattice<vobj> &rhs,int dimension,int shift,int cbmask,
			      std::vector<cobj,alignedAllocator<cobj> > &u_comm_buf,
			      int &u_comm_offset,compressor & compress)
	{
	  typedef typename cobj::vector_type vector_type;
	  typedef typename cobj::scalar_type scalar_type;
	  
	  GridBase *grid=_grid;
	  assert(rhs._grid==_grid);
	  //	  conformable(_grid,rhs._grid);

	  int fd              = _grid->_fdimensions[dimension];
	  int rd              = _grid->_rdimensions[dimension];
	  int simd_layout     = _grid->_simd_layout[dimension];
	  int comm_dim        = _grid->_processors[dimension] >1 ;
	  assert(simd_layout==1);
	  assert(comm_dim==1);
	  assert(shift>=0);
	  assert(shift<fd);
	  
	  int buffer_size = _grid->_slice_nblock[dimension]*_grid->_slice_block[dimension];
	  
	  std::vector<cobj,alignedAllocator<cobj> > send_buf(buffer_size); // hmm...
	  std::vector<cobj,alignedAllocator<cobj> > recv_buf(buffer_size);
	  
	  int cb= (cbmask==0x2)? 1 : 0;
	  int sshift= _grid->CheckerBoardShift(rhs.checkerboard,dimension,shift,cb);
	  
	  for(int x=0;x<rd;x++){       
	    
	    int offnode = ( x+sshift >= rd );
	    int sx        = (x+sshift)%rd;
	    int comm_proc = (x+sshift)/rd;
	    
	    if (offnode) {
	      
	      int words = send_buf.size();
	      if (cbmask != 0x3) words=words>>1;
	    
	      int bytes = words * sizeof(cobj);

	      Gather_plane_simple (rhs,send_buf,dimension,sx,cbmask,compress);

	      int rank           = _grid->_processor;
	      int recv_from_rank;
	      int xmit_to_rank;
	      _grid->ShiftedRanks(dimension,comm_proc,xmit_to_rank,recv_from_rank);
	      
	      //      FIXME Implement asynchronous send & also avoid buffer copy
	      _grid->SendToRecvFrom((void *)&send_buf[0],
				   xmit_to_rank,
				   (void *)&recv_buf[0],
				   recv_from_rank,
				   bytes);
	      printf("GatherStartComms communicated offnode x %d\n",x);fflush(stdout);

	      printf("GatherStartComms inserting %d buf size %d\n",u_comm_offset,buffer_size);fflush(stdout);
	      for(int i=0;i<buffer_size;i++){
		u_comm_buf[u_comm_offset+i]=recv_buf[i];
	      }
	      u_comm_offset+=buffer_size;
	      printf("GatherStartComms inserted x %d\n",x);fflush(stdout);
	    }
	  }
	}


      template<class vobj,class cobj, class compressor> 
	void  GatherStartCommsSimd(Lattice<vobj> &rhs,int dimension,int shift,int cbmask,
				   std::vector<cobj,alignedAllocator<cobj> > &u_comm_buf,
				   int &u_comm_offset,compressor &compress)
	{
	  const int Nsimd = _grid->Nsimd();
	  typedef typename cobj::vector_type vector_type;
	  typedef typename cobj::scalar_type scalar_type;
	  
	  int fd = _grid->_fdimensions[dimension];
	  int rd = _grid->_rdimensions[dimension];
	  int ld = _grid->_ldimensions[dimension];
	  int simd_layout     = _grid->_simd_layout[dimension];
	  int comm_dim        = _grid->_processors[dimension] >1 ;

	  assert(comm_dim==1);
	  assert(simd_layout==2);
	  assert(shift>=0);
	  assert(shift<fd);

	  int permute_type=_grid->PermuteType(dimension);

	  ///////////////////////////////////////////////
	  // Simd direction uses an extract/merge pair
	  ///////////////////////////////////////////////
	  int buffer_size = _grid->_slice_nblock[dimension]*_grid->_slice_block[dimension];
	  int words = sizeof(cobj)/sizeof(vector_type);

	  /*   FIXME ALTERNATE BUFFER DETERMINATION */
	  std::vector<std::vector<scalar_type> > send_buf_extract(Nsimd,std::vector<scalar_type>(buffer_size*words) ); 
	  std::vector<std::vector<scalar_type> > recv_buf_extract(Nsimd,std::vector<scalar_type>(buffer_size*words) );
	  int bytes = buffer_size*words*sizeof(scalar_type);

	  std::vector<scalar_type *> pointers(Nsimd);  //
	  std::vector<scalar_type *> rpointers(Nsimd); // received pointers
	  
	  ///////////////////////////////////////////
	  // Work out what to send where
	  ///////////////////////////////////////////

	  int cb    = (cbmask==0x2)? 1 : 0;
	  int sshift= _grid->CheckerBoardShift(rhs.checkerboard,dimension,shift,cb);
	  
	  std::vector<int> comm_offnode(simd_layout);
	  std::vector<int> comm_proc   (simd_layout);  //relative processor coord in dim=dimension
	  std::vector<int> icoor(_grid->Nd());
	  
	  for(int x=0;x<rd;x++){
	    
	    int comm_any = 0;
	    for(int s=0;s<simd_layout;s++) {
	      int shifted_x   = x+s*rd+sshift;
	      comm_offnode[s] = shifted_x >= ld;
	      comm_any        = comm_any | comm_offnode[s];
	      comm_proc[s]    = shifted_x/ld;
	    }
    
	    int o    = 0;
	    int bo   = x*_grid->_ostride[dimension];
	    int sx   = (x+sshift)%rd;
	    
	    if ( comm_any ) {

	      for(int i=0;i<Nsimd;i++){
		pointers[Nsimd-1-i] = (scalar_type *)&send_buf_extract[i][0];
	      }
	      Gather_plane_extract<cobj>(rhs,pointers,dimension,sx,cbmask,compress);
	      
	      for(int i=0;i<Nsimd;i++){
		
		int s;
		_grid->iCoorFromIindex(icoor,i);
		s = icoor[dimension];
		
		if(comm_offnode[s]){
		  
		  int rank           = _grid->_processor;
		  int recv_from_rank;
		  int xmit_to_rank;

		  _grid->ShiftedRanks(dimension,comm_proc[s],xmit_to_rank,recv_from_rank);
	  

		  _grid->SendToRecvFrom((void *)&send_buf_extract[i][0],
					xmit_to_rank,
					(void *)&recv_buf_extract[i][0],
					recv_from_rank,
					bytes);

		  rpointers[i] = (scalar_type *)&recv_buf_extract[i][0];
		  
		} else { 
		  
		  rpointers[i] = (scalar_type *)&send_buf_extract[i][0];

		}
		
	      }

	      // Permute by swizzling pointers in merge
	      int permute_slice=0;
	      int lshift=sshift%ld;
	      int wrap  =lshift/rd;
	      int  num  =lshift%rd;

	      if ( x< rd-num ) permute_slice=wrap;
	      else permute_slice = 1-wrap;

	      int toggle_bit = (Nsimd>>(permute_type+1));
	      int PermuteMap;
	      for(int i=0;i<Nsimd;i++){
		if ( permute_slice ) {
		  PermuteMap=i^toggle_bit;
		  pointers[Nsimd-1-i] = rpointers[PermuteMap];
		} else {
		  pointers[Nsimd-1-i] = rpointers[i];
		}
	      }

	      // Here we don't want to scatter, just place into a buffer.
	      for(int i=0;i<buffer_size;i++){
		merge(u_comm_buf[u_comm_offset+i],pointers);
	      }

	    }
	  }
	}
  };
}
#endif
