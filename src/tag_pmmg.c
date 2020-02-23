/* =============================================================================
**  This file is part of the parmmg software package for parallel tetrahedral
**  mesh modification.
**  Copyright (c) Bx INP/Inria/UBordeaux, 2017-
**
**  parmmg is free software: you can redistribute it and/or modify it
**  under the terms of the GNU Lesser General Public License as published
**  by the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  parmmg is distributed in the hope that it will be useful, but WITHOUT
**  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
**  FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
**  License for more details.
**
**  You should have received a copy of the GNU Lesser General Public
**  License and of the GNU General Public License along with parmmg (in
**  files COPYING.LESSER and COPYING). If not, see
**  <http://www.gnu.org/licenses/>. Please read their terms carefully and
**  use this copy of the parmmg distribution only if you accept them.
** =============================================================================
*/

/**
 * \file tag_pmmg.c
 * \brief Functions related to boundary and interface tags
 * \author Luca Cirrottola (Inria)
 * \version 5
 * \copyright GNU Lesser General Public License.
 */
#include "parmmg.h"


/**
 * \param ppt pointer to the point.
 *
 * Tag a node as parallel.
 */
void PMMG_tag_par_node(MMG5_pPoint ppt){

  ppt->tag |= (MG_PARBDY + MG_BDY + MG_REQ + MG_NOSURF);
}

/**
 * \param pxt pointer to the xtetra.
 * \param j local index of the edge on the tetra.
 *
 * Tag an edge as parallel.
 */
void PMMG_tag_par_edge(MMG5_pxTetra pxt,int j){

  pxt->tag[j] |= (MG_PARBDY + MG_BDY + MG_REQ + MG_NOSURF);
}

/**
 * \param pt pointer to the tetra.
 * \param hash edges hash table.
 * \param ia local index of the edge on the tetra.
 *
 * Tag an edge as parallel.
 */
int PMMG_tag_par_edge_hash(MMG5_pTetra pt,MMG5_HGeom hash,int ia){
  int ip0,ip1;

  ip0 = pt->v[MMG5_iare[ia][0]];
  ip1 = pt->v[MMG5_iare[ia][1]];
  if( !MMG5_hTag( &hash, ip0, ip1, 0,
                  MG_PARBDY + MG_BDY + MG_REQ + MG_NOSURF ) ) return 0;

  return 1;
}

/**
 * \param pxt pointer to the xtetra.
 * \param j local index of the face on the tetra.
 *
 * Tag a face as parallel.
 */
void PMMG_tag_par_face(MMG5_pxTetra pxt,int j){

  pxt->ftag[j] |= (MG_PARBDY + MG_BDY + MG_REQ + MG_NOSURF);
}


/**
 * \param pxt pointer to the point.
 *
 * Untag a parallel node.
 */
void PMMG_untag_par_node(MMG5_pPoint ppt){

  if ( ppt->tag & MG_PARBDY ) {
    ppt->tag &= ~MG_PARBDY;
    if ( ppt->tag & MG_BDY )    ppt->tag &= ~MG_BDY;
    if ( ppt->tag & MG_REQ )    ppt->tag &= ~MG_REQ;
    if ( ppt->tag & MG_NOSURF ) ppt->tag &= ~MG_NOSURF;
  }
}

/**
 * \param pxt pointer to the xtetra.
 * \param j local index of the edge on the tetra.
 *
 * Untag a parallel edge.
 */
void PMMG_untag_par_edge(MMG5_pxTetra pxt,int j){

  if ( pxt->tag[j] & MG_PARBDY ) {
    pxt->tag[j] &= ~MG_PARBDY;
    if ( pxt->tag[j] & MG_BDY)    pxt->tag[j] &= ~MG_BDY;
    if ( pxt->tag[j] & MG_REQ)    pxt->tag[j] &= ~MG_REQ;
    if ( pxt->tag[j] & MG_NOSURF) pxt->tag[j] &= ~MG_NOSURF;
  }
}

/**
 * \param pxt pointer to the xtetra.
 * \param j local index of the face on the tetra.
 *
 * Untag a parallel face.
 */
void PMMG_untag_par_face(MMG5_pxTetra pxt,int j){

  if ( pxt->ftag[j] & MG_PARBDY ) {
    pxt->ftag[j] &= ~MG_PARBDY;
    if ( pxt->ftag[j] & MG_BDY)    pxt->ftag[j] &= ~MG_BDY;
    if ( pxt->ftag[j] & MG_REQ)    pxt->ftag[j] &= ~MG_REQ;
    if ( pxt->ftag[j] & MG_NOSURF) pxt->ftag[j] &= ~MG_NOSURF;
  }
}


/**
 * \param parmesh pointer toward the parmesh structure.
 *
 * \return 0 if fail, 1 otherwise
 *
 * Update the tag on the points and tetra
 *
 */
int PMMG_updateTag(PMMG_pParMesh parmesh) {
  PMMG_pGrp       grp;
  MMG5_pMesh      mesh;
  MMG5_pTetra     pt;
  MMG5_pxTetra    pxt;
  MMG5_pPoint     ppt;
  MMG5_HGeom      hash;
  int             *node2int_node_comm0_index1,*face2int_face_comm0_index1;
  int             grpid,iel,ifac,ia,ip0,ip1,k,j,i,getref;
  size_t          available,oldMemMax;

  /* Compute available memory (previously given to the communicators) */
  PMMG_TRANSFER_AVMEM_TO_PARMESH(parmesh,available,oldMemMax);

  /* Loop on groups */
  for ( grpid=0; grpid<parmesh->ngrp; grpid++ ) {
    grp                        = &parmesh->listgrp[grpid];
    mesh                       = grp->mesh;
    node2int_node_comm0_index1 = grp->node2int_node_comm_index1;
    face2int_face_comm0_index1 = grp->face2int_face_comm_index1;

    PMMG_TRANSFER_AVMEM_FROM_PMESH_TO_MESH(parmesh,mesh,available,oldMemMax);

    /** Step 1: Loop on xtetras to untag old parallel entities, then build
     * hash table for edges on xtetras. */
    for ( k=1; k<=mesh->ne; k++ ) {
      pt = &mesh->tetra[k];
      if ( !pt->xt ) continue;
      pxt = &mesh->xtetra[pt->xt];
      /* Untag parallel nodes */
      for ( j=0 ; j<4 ; j++ ) {
        ppt = &mesh->point[pt->v[j]];
        PMMG_untag_par_node(ppt);
      }
      /* Untag parallel edges */
      for ( j=0 ; j<6 ; j++ )
        PMMG_untag_par_edge(pxt,j);
      /* Untag parallel faces */
      for ( j=0 ; j<4 ; j++ )
        PMMG_untag_par_face(pxt,j);
    }

    /* Create hash table for edges */
    if ( !MMG5_hNew(mesh, &hash, 6*mesh->xt, 8*mesh->xt) ) return 0;
    for ( k=1; k<=mesh->ne; k++ ) {
      pt = &mesh->tetra[k];
      if ( !pt->xt ) continue;
      for ( j=0; j<6; j++ ) {
        ip0 = pt->v[MMG5_iare[j][0]];
        ip1 = pt->v[MMG5_iare[j][1]];
        if( !MMG5_hEdge( mesh, &hash, ip0, ip1, 0, MG_NOTAG ) ) return 0;
      }
    }

    /** Step 2: Re-tag boundary entities starting from xtetra faces. */
    for ( k=1; k<=mesh->ne; k++ ) {
      pt = &mesh->tetra[k];
      if ( !pt->xt ) continue;
      pxt = &mesh->xtetra[pt->xt];
      /* Look for external boundary faces (MG_BDY) or internal boundary faces
       * previously on parallel interfaces (MG_PARBDYBDY), tag their edges and
       * nodes (the BDY tag could have been removed when deleting old parallel
       * interfaces in step 1).*/
      for ( ifac=0 ; ifac<4 ; ifac++ ) {
        if ( pxt->ftag[ifac] & MG_PARBDYBDY ) {
          pxt->ftag[ifac] &= ~MG_PARBDYBDY;
          pxt->ftag[ifac] |= MG_BDY;
        }
        /* Only a "true" boundary after this line */
        if ( pxt->ftag[ifac] & MG_BDY ) {
          /* Constrain boundary if -nosurf option */
          if( mesh->info.nosurf ) pxt->ftag[ifac] |= MG_REQ + MG_NOSURF;
          /* Tag face edges */
          for ( j=0; j<3; j++ ) {
            ia = MMG5_iarf[ifac][j];
            ip0 = pt->v[MMG5_iare[ia][0]];
            ip1 = pt->v[MMG5_iare[ia][1]];
            if( !MMG5_hTag( &hash, ip0, ip1, 0, MG_BDY ) ) return 0;
            /* Constrain boundary if -nosurf option */
            if( mesh->info.nosurf )
              if( !MMG5_hTag( &hash, ip0, ip1, 0, MG_REQ + MG_NOSURF ) ) return 0;
          }
          /* Tag face nodes */
          for ( j=0 ; j<3 ; j++) {
            ppt = &mesh->point[pt->v[MMG5_idir[ifac][j]]];
            ppt->tag |= MG_BDY;
            /* Constrain boundary if -nosurf option */
            if( mesh->info.nosurf ) ppt->tag |= MG_REQ + MG_NOSURF;
          }
        }
      }
    }

    /** Step 3: Tag new parallel interface entities starting from int_face_comm.
     *
     *  This step needs to be done even if the external face communicator is
     *  not allocated (for example, when a centralized mesh is loaded and split
     *  on a single proc: in this case the internal communicator is allocated,
     *  but not the external one).
     */
    for ( i=0; i<grp->nitem_int_face_comm; i++ ) {
      iel  =   face2int_face_comm0_index1[i] / 12;
      ifac = ( face2int_face_comm0_index1[i] % 12 ) / 3;
      pt = &mesh->tetra[iel];
      assert( pt->xt );
      pxt = &mesh->xtetra[pt->xt];
      /* If already boundary, make it recognizable as a "true" boundary */
      if( pxt->ftag[ifac] & MG_BDY ) pxt->ftag[ifac] |= MG_PARBDYBDY;
      /* Tag face */
      PMMG_tag_par_face(pxt,ifac);
      /* Tag face edges */
      for ( j=0; j<3; j++ ) {
        ia = MMG5_iarf[ifac][j];
        if( !PMMG_tag_par_edge_hash(pt,hash,ia) ) return 0;
      }
      /* Tag face nodes */
      for ( j=0 ; j<3 ; j++) {
        ppt = &mesh->point[pt->v[MMG5_idir[ifac][j]]];
        PMMG_tag_par_node(ppt);
      }
    }

    /** Step 4: Get edge tag and delete hash table */
    for ( k=1; k<=mesh->ne; k++ ) {
      pt = &mesh->tetra[k];
      if ( !pt->xt ) continue;
      pxt = &mesh->xtetra[pt->xt];
      for ( j=0; j<6; j++ ) {
        ip0 = pt->v[MMG5_iare[j][0]];
        ip1 = pt->v[MMG5_iare[j][1]];
        /* Put the tag stored in the hash table on the xtetra edge */
        if( !MMG5_hGet( &hash, ip0, ip1, &getref, &pxt->tag[j] ) ) return 0;
      }
    }
    PMMG_DEL_MEM( mesh, hash.geom, MMG5_hgeom, "Edge hash table" );

    /** Step 5: Unreference xpoints not on BDY (or PARBDY) */
    for ( i=1; i<=mesh->np; i++ ) {
      ppt = &mesh->point[i];
      if( ppt->tag & MG_BDY ) continue;
      if( ppt->xp ) ppt->xp = 0;
    }

    PMMG_TRANSFER_AVMEM_FROM_MESH_TO_PMESH(parmesh,mesh,available,oldMemMax);
  }

  return 1;
}

/**
 * \param parmesh pointer to parmesh structure.
 * \return 0 if fail, 1 if success.
 *
 * Check if faces on a parallel communicator connect elements with different
 * references, and tag them as a "true" boundary (thus PARBDYBDY).
 */
int PMMG_parbdySet( PMMG_pParMesh parmesh ) {
  PMMG_pGrp      grp;
  PMMG_pExt_comm ext_face_comm;
  PMMG_pInt_comm int_face_comm;
  MMG5_pMesh     mesh;
  MMG5_pTetra    pt;
  MMG5_pxTetra   pxt;
  MPI_Comm       comm;
  MPI_Status     status;
  int            *face2int_face_comm_index1,*face2int_face_comm_index2;
  int            *seenFace,*intvalues,*itosend,*itorecv;
  int            ngrp,myrank,color,nitem,k,igrp,i,idx,ie,ifac;

  comm   = parmesh->comm;
  grp    = parmesh->listgrp;
  myrank = parmesh->myrank;
  ngrp   = parmesh->ngrp;

  /* intvalues will be used to store tetra ref */
  int_face_comm = parmesh->int_face_comm;
  PMMG_MALLOC(parmesh,int_face_comm->intvalues,int_face_comm->nitem,int,
              "intvalues",return 0);
  intvalues = parmesh->int_face_comm->intvalues;

  /* seenFace will be used to recognize already visited faces */
  PMMG_CALLOC(parmesh,seenFace,int_face_comm->nitem,int,"seenFace",return 0);

  /** Fill the internal communicator with the first ref found */
  for( igrp = 0; igrp < ngrp; igrp++ ) {
    grp                       = &parmesh->listgrp[igrp];
    mesh                      = grp->mesh;
    face2int_face_comm_index1 = grp->face2int_face_comm_index1;
    face2int_face_comm_index2 = grp->face2int_face_comm_index2;

    for ( k=0; k<grp->nitem_int_face_comm; ++k ) {
      ie   =  face2int_face_comm_index1[k]/12;
      ifac = (face2int_face_comm_index1[k]%12)/3;
      idx  =  face2int_face_comm_index2[k];
      pt = &mesh->tetra[ie];
      assert( MG_EOK(pt) && pt->xt );
      pxt = &mesh->xtetra[pt->xt];

      /* Tag face as "true" boundary if its second ref is different */
      if( !seenFace[idx] )
        intvalues[idx] = pt->ref;
      else if( intvalues[idx] != pt->ref )
        pxt->ftag[ifac] |= MG_PARBDYBDY;

      /* Mark face each time that it's seen */
      seenFace[idx]++;
    }
  }

  /** Send and receive external communicators filled with the tetra ref */
  for ( k=0; k<parmesh->next_face_comm; ++k ) {
    ext_face_comm = &parmesh->ext_face_comm[k];
    nitem         = ext_face_comm->nitem;
    color         = ext_face_comm->color_out;

    PMMG_CALLOC(parmesh,ext_face_comm->itosend,nitem,int,"itosend array",
                return 0);
    itosend = ext_face_comm->itosend;

    PMMG_CALLOC(parmesh,ext_face_comm->itorecv,nitem,int,"itorecv array",
                return 0);
    itorecv = ext_face_comm->itorecv;

    for ( i=0; i<nitem; ++i ) {
      idx            = ext_face_comm->int_comm_index[i];
      itosend[i]     = intvalues[idx];
    }

    MPI_CHECK(
      MPI_Sendrecv(itosend,nitem,MPI_INT,color,MPI_COMMUNICATORS_REF_TAG,
                   itorecv,nitem,MPI_INT,color,MPI_COMMUNICATORS_REF_TAG,
                   comm,&status),return 0 );

    /* Store the info in intvalues */
    for ( i=0; i<nitem; ++i ) {
      idx            = ext_face_comm->int_comm_index[i];
      intvalues[idx] = itorecv[i];
    }
  }

  /* Check the internal communicator */
  for( igrp = 0; igrp < ngrp; igrp++ ) {
    grp                       = &parmesh->listgrp[igrp];
    mesh                      = grp->mesh;
    face2int_face_comm_index1 = grp->face2int_face_comm_index1;
    face2int_face_comm_index2 = grp->face2int_face_comm_index2;

    for ( k=0; k<grp->nitem_int_face_comm; ++k ) {
      ie   =  face2int_face_comm_index1[k]/12;
      ifac = (face2int_face_comm_index1[k]%12)/3;
      idx  =  face2int_face_comm_index2[k];
      pt = &mesh->tetra[ie];
      assert( MG_EOK(pt) && pt->xt );
      pxt = &mesh->xtetra[pt->xt];

      /* Faces on the external communicator have been visited only once */
      if( seenFace[idx] != 1 ) continue;

      /* Tag face as "true" boundary if its ref is different */
      if( intvalues[idx] != pt->ref )
        pxt->ftag[ifac] |= MG_PARBDYBDY;
    }
  }

  /* Deallocate and return */
  PMMG_DEL_MEM(parmesh,int_face_comm->intvalues,int,"intvalues");
  PMMG_DEL_MEM(parmesh,seenFace,int,"seenFace");
  for ( k=0; k<parmesh->next_face_comm; ++k ) {
    ext_face_comm = &parmesh->ext_face_comm[k];
    PMMG_DEL_MEM(parmesh,ext_face_comm->itosend,int,"itosend array");
    PMMG_DEL_MEM(parmesh,ext_face_comm->itorecv,int,"itorecv array");
  }

  return 1;
}
