#include "debug_pmmg.h"
#include <stdio.h> // fopen, fclose, fprintf
#include <stdlib.h> // abort
#ifdef __linux__
#include <malloc.h> // mallinfo
#endif

static FILE* my_fopen( char *name, char *status )
{
   FILE *fp = fopen( name, status );
   if ( NULL == fp ) {
     fprintf( stderr, "error opening: %s. File: %s, line: %d \n", name, __FILE__, __LINE__);
     abort();
   }
   return fp;
}

void grplst_meshes_to_txt( char *name, PMMG_pGrp grp, int ngrp )
{
  FILE *fp = my_fopen( name, "w" );
  for ( int imsh = 0; imsh < ngrp; ++imsh ) {
    fprintf( fp, "Points in mesh %d\n", imsh );
    for ( int k = 0; k < grp[imsh].mesh->np + 2; k++ ) {
      fprintf( fp,
          "\tid#\t%10d\tcoords:\t(%9.6f,%9.6f,%9.6f )"
          "\tnormals:\t(%9.6f,%9.6f,%9.6f )"
          "\tref:\t%5d\t\txp\t%10d\ttmp\t%5d\ttag\t%5d",
          k,
          grp[ imsh ].mesh->point[ k ].c[ 0 ],
          grp[ imsh ].mesh->point[ k ].c[ 1 ],
          grp[ imsh ].mesh->point[ k ].c[ 2 ],
          grp[ imsh ].mesh->point[ k ].n[ 0 ],
          grp[ imsh ].mesh->point[ k ].n[ 1 ],
          grp[ imsh ].mesh->point[ k ].n[ 2 ],
          grp[ imsh ].mesh->point[ k ].ref,
          grp[ imsh ].mesh->point[ k ].xp,
          grp[ imsh ].mesh->point[ k ].tmp,
          grp[ imsh ].mesh->point[ k ].tag);
      if ( grp[ imsh ].mesh->point[ k ].xp != 0 )
        fprintf( fp, "\t\txp\t%10d\tn1\t(%9.6f,%9.6f,%9.6f )\tn2\t(%9.6f,%9.6f,%9.6f )",
            grp[ imsh ].mesh->point[ k ].xp,
            grp[ imsh ].mesh->xpoint[ grp[ imsh ].mesh->point[ k ].xp ].n1[ 0 ],
            grp[ imsh ].mesh->xpoint[ grp[ imsh ].mesh->point[ k ].xp ].n1[ 1 ],
            grp[ imsh ].mesh->xpoint[ grp[ imsh ].mesh->point[ k ].xp ].n1[ 2 ],
            grp[ imsh ].mesh->xpoint[ grp[ imsh ].mesh->point[ k ].xp ].n2[ 0 ],
            grp[ imsh ].mesh->xpoint[ grp[ imsh ].mesh->point[ k ].xp ].n2[ 1 ],
            grp[ imsh ].mesh->xpoint[ grp[ imsh ].mesh->point[ k ].xp ].n2[ 2 ]);
      fprintf( fp, "\n" );
    }
  }
  fclose(fp);
}

void tetras_of_mesh_to_txt( char *name, MMG5_pMesh mesh, int num )
{
  FILE *fp = my_fopen( name, "w" );
  fprintf( fp, "Tetras in  mesh %d.ne: %d, nei:%d\n", num, mesh->ne, mesh->nei );
  for ( int k = 1; k < mesh->ne + 2; ++k )
    fprintf( fp,
        "%d %d %d %d\n",
        mesh->tetra->v[0],  mesh->tetra->v[1], mesh->tetra->v[2], mesh->tetra->v[3] );
  fclose(fp);
}

void find_tetras_referencing_null_points_to_txt( char *name, PMMG_pGrp grp, int nmsh )
{
  FILE *fp = my_fopen( name, "w" );
  for ( int imsh = 0; imsh < nmsh; ++imsh ) {
    for ( int tet = 0; tet < grp[imsh].mesh->ne; ++tet ) {
      int check = 0;
      for ( int k = 0; k < 4; ++k )
        if ( 0 == grp[imsh].mesh->tetra[tet].v[k] )
          ++check;
      if ( 3 == check )
        fprintf(fp, " mesh %d references point %d with all zero coordinates \n",imsh, tet );
      check = 0;
    }
  }
  fclose(fp);
}

int adja_idx_of_face( MMG5_pMesh mesh, int element, int face )
{
  int location = 4 * (element - 1) + 1 + face;
  int max_loc = 4 * (mesh->ne-1) + 5;

  assert( (face >= 0)    && (face < 4) && "There are only 4 faces per tetra" );
  assert( (location > 0) && (location < max_loc) && " adja out of bound "  );
  return mesh->adja[ location ];
}
int adja_tetra_to_face( MMG5_pMesh mesh, int element, int face )
{
  return adja_idx_of_face( mesh, element, face ) / 4;
}
int adja_face_to_face( MMG5_pMesh mesh, int element, int face )
{
  return adja_idx_of_face( mesh, element, face ) % 4;
}

void listgrp_meshes_adja_of_tetras_to_txt( char *name, PMMG_pGrp grp, int ngrp )
{
  FILE *fp = my_fopen( name, "w" );
  for ( int imsh = 0; imsh < ngrp; ++imsh ) {
    fprintf( fp, "Mesh %d, ne= %d\n", imsh, grp[imsh].mesh->ne );
    for ( int k = 1; k < grp[imsh].mesh->ne + 1; ++k ) {
      fprintf( fp, "tetra %d\t\t", k );
      for ( int i = 0; i < 4; ++i ) {
        fprintf( fp, "adja[%d] %d", i, adja_idx_of_face( grp[ imsh ].mesh, k, i ) );
        fprintf( fp, ", (tetra:%d, face:%1d)\t",
            adja_tetra_to_face( grp[ imsh ].mesh, k, i ),
            adja_face_to_face( grp[ imsh ].mesh, k, i ) );
      }
      fprintf( fp, "\n" );
    }
  }
  fclose(fp);
}

void grplst_meshes_to_saveMesh( PMMG_pGrp listgrp, int ngrp, int rank, char *basename )
{
  int grpId;
  char name[ 2048 ];
  assert( ( strlen( basename ) < 2048 - 14 ) && "filename too big" );

  for ( grpId = 0 ; grpId < ngrp ; grpId++ ) {
    //MMG5_saveMshMesh( pmesh[ grpId ].mesh, mesMMG5_pSol met,const char *filename)
    sprintf( name, "%s-P%02d-%02d.mesh", basename, rank, grpId );
    MMG3D_hashTetra( listgrp[ grpId ].mesh, 1 );
    MMG3D_bdryBuild( listgrp[ grpId ].mesh ); //note: no error checking
    MMG3D_saveMesh( listgrp[ grpId ].mesh, name );
    if ( listgrp[ grpId ].met->m ) {
      sprintf( name, "%s-P%02d-%02d.sol", basename, rank, grpId );
      MMG3D_saveSol( listgrp[ grpId ].mesh, listgrp[ grpId ].met, name );
    }
  }
}

void dump_malloc_allocator_info( char *msg, int id )
{
  char name[ 16 ];
  FILE *fp;

  sprintf(name,"mem_info-%02d.txt", id );
  fp = my_fopen( name, "a" );

#ifdef __linux__
  const int mb = 1024 * 1024;
  struct mallinfo me = mallinfo();

  fprintf( fp, "%s \n", msg );
  fprintf( fp, "** MALLOC ALLOCATOR INFO ***************************\n" );
  fprintf( fp, "* %4d \tNon-mmapped space allocated (mbytes)       *\n", me.arena / mb );
  fprintf( fp, "* %4d \tNumber of free chunks                      *\n", me.ordblks );
  fprintf( fp, "* %4d \tNumber of free fastbin blocks              *\n", me.smblks );
  fprintf( fp, "* %4d \tNumber of mmapped regions                  *\n", me.hblks );
  fprintf( fp, "* %4d \tSpace allocated in mmapped regions (mbytes)*\n", me.hblkhd / mb );
  fprintf( fp, "* %4d \tMaximum total allocated space (mbytes)     *\n", me.usmblks / mb );
  fprintf( fp, "* %4d \tSpace in freed fastbin blocks (mbytes)     *\n", me.fsmblks / mb );
  fprintf( fp, "* %4d \tTotal allocated space (mbytes)             *\n", me.uordblks / mb );
  fprintf( fp, "* %4d \tTotal free space (mbytes)                  *\n", me.fordblks / mb );
  fprintf( fp, "* %4d \tTop-most, releasable space (mbytes)        *\n", me.keepcost / mb );
  fprintf( fp, "****************************************************\n\n" );

  fclose(fp);
#else
  fprintf( fp, "Extended information read directly from the malloc allocator is"
      "currently only implemented on linux\n" );
#endif
}

void check_mem_max_and_mem_cur( PMMG_pParMesh parmesh, const char *msg )
{
  size_t n_total = parmesh->memCur;
  const size_t mb = 1024 * 1024;
  for ( size_t i = 0; i < parmesh->ngrp; ++i )
    n_total += parmesh->listgrp[ i ].mesh->memCur;
  if ( n_total > parmesh->memGloMax )
    fprintf( stderr,
             "%2d-%2d: %s: memCur check ERROR: memCur ( %8.2fMb ) > memGloMax ( %8.2fMb ) at %s %s %d\n",
	     parmesh->myrank, parmesh->nprocs, msg,
	     n_total / (float) mb, parmesh->memGloMax / (float) mb,
	     __func__, __FILE__, __LINE__ );
//  else
//    fprintf( stderr,
//             "%2d-%2d: %s: memCur check OK: memCur = %8.2fMb - memGloMax = %8.2fMb \n",
//             parmesh->myrank, parmesh->nprocs, mesg,
//             n_total / (float) mb, parmesh->memGloMax / (float) mb );

  n_total = parmesh->memMax;
  for ( size_t i = 0; i < parmesh->ngrp; ++i )
    n_total += parmesh->listgrp[ i ].mesh->memMax;
  if ( n_total > parmesh->memGloMax )
    fprintf( stderr,
             "%2d-%2d: %s: memMax check ERROR: memMax ( %8.2fMb ) > memGloMax ( %8.2fMb ) at %s %s %d\n",
             parmesh->myrank, parmesh->nprocs, msg,
	     n_total / (float) mb, parmesh->memGloMax / (float) mb,
	     __func__, __FILE__, __LINE__ );
//  else
//    fprintf( stderr,
//             "%2d-%2d: %s: memMax check OK: memMax = %8.2fMb - memGloMax = %8.2fMb \n",
//             parmesh->myrank, parmesh->nprocs, msg,
//             n_total / (float) mb, parmesh->memGloMax / (float) mb );
}
