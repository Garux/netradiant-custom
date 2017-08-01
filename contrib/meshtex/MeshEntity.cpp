/**
 * @file MeshEntity.cpp
 * Implements the MeshEntity class.
 * @ingroup meshtex-core
 */

/*
 * Copyright 2012 Joel Baxter
 *
 * This file is part of MeshTex.
 *
 * MeshTex is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * MeshTex is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with MeshTex.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cmath>

#include "MeshEntity.h"
#include "MeshEntityMessages.h"

#include "ishaders.h"
#include "texturelib.h"


/**
 * Size of buffer for composing messages to send to the info callback.
 */
#define INFO_BUFFER_SIZE 1024

/**
 * Stop successive refinement of path length estimates when the change in values
 * is equal to or less than this tolerance.
 */
#define UNITS_ERROR_BOUND 0.5

/**
 * Macro to get ROW_SLICE_TYPE from COL_SLICE_TYPE and vice versa, used in
 * code that can operate on either kind of slice. This does mean that the
 * numerical values assigned to ROW_SLICE_TYPE and COL_SLICE_TYPE are
 * meaningful.
 *
 * @param sliceType Kind of slice to find the other of, so to speak.
 */
#define OtherSliceType(sliceType) (1 - (sliceType))

/**
 * Macro to find the number of control points in a slice, which is equal to
 * the number of slices of the other kind.
 *
 * @param sliceType Kind of slice to measure.
 */
#define SliceSize(sliceType) (_numSlices[OtherSliceType(sliceType)])

/**
 * Macro to get rid of negative-zero values.
 *
 * @param floatnum Number to be sanitized.
 */
#define SanitizeFloat(floatnum) ((floatnum) == -0.0f ? 0.0f : (floatnum))


/**
 * For a given slice kind, which texture axis (S or T) normally changes
 * along it.
 */
MeshEntity::TextureAxis MeshEntity::_naturalAxis[NUM_SLICE_TYPES] =
   { S_TEX_AXIS, T_TEX_AXIS };

/**
 * For a given slice kind, whether Radiant's "natural" scale along a texture
 * axis is backwards compared to the progression of the indices of the
 * orthogonal slices.
 */
bool MeshEntity::_radiantScaleInverted[NUM_SLICE_TYPES] = { false, true };

/**
 * For a given slice kind, whether Radiant's interpretation of tiling along a
 * texture axis is backwards compared to the progression of the indices of
 * the orthogonal slices.
 */
bool MeshEntity::_radiantTilesInverted[NUM_SLICE_TYPES] = { false, false };

/**
  * Message format strings for describing texture mapping on a slice.
  */
const char *MeshEntity::_infoSliceFormatString[NUM_SLICE_TYPES] =
   { INFO_ROW_FORMAT, INFO_COL_FORMAT };

/**
 * Message format strings for describing texture mapping on a slice in the
 * unusual case where the scale value is infinite.
 */
const char *MeshEntity::_infoSliceInfscaleFormatString[NUM_SLICE_TYPES] =
   { INFO_ROW_INFSCALE_FORMAT, INFO_COL_INFSCALE_FORMAT };

/**
 * Message format strings for warning that a scale value is infinite and
 * cannot be transferred to the Set S/T Scale dialog.
 */
const char *MeshEntity::_warningSliceInfscaleFormatString[NUM_SLICE_TYPES] =
   { WARNING_ROW_INFSCALE, WARNING_COL_INFSCALE };

/**
 * Message format strings for an illegal slice number error.
 */
const char *MeshEntity::_errorBadSliceString[NUM_SLICE_TYPES] =
   { ERROR_BAD_ROW, ERROR_BAD_COL };

/**
 * Message format strings for a scale = 0 error.
 */
const char *MeshEntity::_errorSliceZeroscaleString[NUM_SLICE_TYPES] =
   { ERROR_ROW_ZEROSCALE, ERROR_COL_ZEROSCALE };

/**
 * Message format strings for a tiles = 0 error.
 */
const char *MeshEntity::_errorSliceZerotilesString[NUM_SLICE_TYPES] =
   { ERROR_ROW_ZEROTILES, ERROR_COL_ZEROTILES };


/**
 * Constructor. If the constructor is unable to process the input mesh, then
 * the internal valid flag (queryable through IsValid) is set false, and the
 * errorReportCallback is invoked.
 *
 * @param mesh                  The patch mesh to construct a wrapper for.
 * @param infoReportCallback    Callback for future informational messages.
 * @param warningReportCallback Callback for future warning messages.
 * @param errorReportCallback   Callback for future error messages.
 */
MeshEntity::MeshEntity(scene::Node& mesh,
                       const MessageCallback& infoReportCallback,
                       const MessageCallback& warningReportCallback,
                       const MessageCallback& errorReportCallback) :
   _mesh(mesh),
   _infoReportCallback(infoReportCallback),
   _warningReportCallback(warningReportCallback),
   _errorReportCallback(errorReportCallback)
{
   // Get a handle on the control points, for future manipulation.
   _meshData = GlobalPatchCreator().Patch_getControlPoints(_mesh);
   // Record some useful characteristics of the mesh.
   _numSlices[ROW_SLICE_TYPE] = static_cast<int>(_meshData.x());
   _numSlices[COL_SLICE_TYPE] = static_cast<int>(_meshData.y());
   const char *shaderName = GlobalPatchCreator().Patch_getShader(_mesh);
   IShader *shader = GlobalShaderSystem().getShaderForName(shaderName);
   qtexture_t *texture = shader->getTexture();
   if (texture != NULL)
   {
      _naturalTexUnits[S_TEX_AXIS] = texture->width / 2.0f;
      _naturalTexUnits[T_TEX_AXIS] = texture->height / 2.0f;
   }
   // We don't need the shader for anything else now.
   shader->DecRef();
   // Check for valid mesh; bail if not.
   if (_numSlices[ROW_SLICE_TYPE] < 3 ||
       _numSlices[COL_SLICE_TYPE] < 3 ||
       texture == NULL)
   {
      _valid = false;
      _errorReportCallback(ERROR_BAD_MESH);
      return;
   }
   _valid = true;
   // Find the worldspace extents of the mesh now... they won't change during
   // the lifetime of this object.
   UpdatePosMinMax(X_POS_AXIS);
   UpdatePosMinMax(Y_POS_AXIS);
   UpdatePosMinMax(Z_POS_AXIS);
   // We'll calculate the S/T extents lazily.
   _texMinMaxDirty[S_TEX_AXIS] = true;
   _texMinMaxDirty[T_TEX_AXIS] = true;
}

/**
 * Destructor. Note that this only destroys the wrapper object, not the patch
 * mesh itself.
 */
MeshEntity::~MeshEntity()
{
}

/**
 * Query if the patch mesh is valid, in the characteristics that this wrapper
 * class cares about. If not valid then the results of operations on this
 * wrapper object are undefined.
 *
 * @return true if valid, false if not.
 */
bool
MeshEntity::IsValid() const
{
   return _valid;
}

/**
 * Get information about the patch mesh.
 * 
 * A message string describing general mesh information (number of rows/cols,
 * min/max texture coords, extent in worldspace) will be composed and sent to
 * the infoReportCallback that was specified when this wrapper object was
 * constructed.
 * 
 * Optionally this method can do additional reporting on a specific
 * "reference row" and "reference column". If a reference row and/or column
 * is specified, then information about the reference slice(s) will be added
 * to the information message. If a reference row/col is specified AND a
 * corresponding row/col TexInfoCallback is specified, then the scale and
 * tiling values for the reference slice will also be passed to the relevant
 * callback.
 *
 * @param refRow             Pointer to reference row number; NULL if none.
 * @param refCol             Pointer to reference column number; NULL if none.
 * @param rowTexInfoCallback Pointer to callback for reference row info; NULL
 *                           if none.
 * @param colTexInfoCallback Pointer to callback for reference column info; NULL
 *                           if none.
 */
void
MeshEntity::GetInfo(const int *refRow,
                    const int *refCol,
                    const TexInfoCallback *rowTexInfoCallback,
                    const TexInfoCallback *colTexInfoCallback)
{
   // Prep a message buffer to compose the response.
   char messageBuffer[INFO_BUFFER_SIZE + 1];
   messageBuffer[INFO_BUFFER_SIZE] = 0;
   size_t bufferOffset = 0;
   // Get reference row info if requested; this will be written into the message
   // buffer as well as sent to the row callback (if any).
   if (refRow != NULL)
   {
      ReportSliceTexInfo(ROW_SLICE_TYPE, *refRow, _naturalAxis[ROW_SLICE_TYPE],
                         messageBuffer + bufferOffset,
                         INFO_BUFFER_SIZE - bufferOffset,
                         rowTexInfoCallback);
      // Move the message buffer pointer along.
      bufferOffset = strlen(messageBuffer);
   }
   // Get reference column info if requested; this will be written into the
   // message buffer as well as sent to the column callback (if any).
   if (refCol != NULL)
   {
      ReportSliceTexInfo(COL_SLICE_TYPE, *refCol, _naturalAxis[COL_SLICE_TYPE],
                         messageBuffer + bufferOffset,
                         INFO_BUFFER_SIZE - bufferOffset,
                         colTexInfoCallback);
      // Move the message buffer pointer along.
      bufferOffset = strlen(messageBuffer);
   }
   // Make sure we have up-to-date S/T extents.
   UpdateTexMinMax(S_TEX_AXIS);
   UpdateTexMinMax(T_TEX_AXIS);
   // Add general mesh info to the message.
   snprintf(messageBuffer + bufferOffset, INFO_BUFFER_SIZE - bufferOffset,
            INFO_MESH_FORMAT,
            _numSlices[ROW_SLICE_TYPE],
            SanitizeFloat(_texMin[S_TEX_AXIS]), SanitizeFloat(_texMax[S_TEX_AXIS]),
            _numSlices[COL_SLICE_TYPE],
            SanitizeFloat(_texMin[T_TEX_AXIS]), SanitizeFloat(_texMax[T_TEX_AXIS]),
            SanitizeFloat(_posMin[X_POS_AXIS]), SanitizeFloat(_posMax[X_POS_AXIS]),
            SanitizeFloat(_posMin[Y_POS_AXIS]), SanitizeFloat(_posMax[Y_POS_AXIS]),
            SanitizeFloat(_posMin[Z_POS_AXIS]), SanitizeFloat(_posMax[Z_POS_AXIS]));
   // Send the response.
   _infoReportCallback(messageBuffer);
}

/**
 * For each of the specified texture axes, shift the lowest-valued texture
 * coordinates off of the mesh until an integral texture coordinate (texture
 * boundary) is on the mesh edge.
 *
 * @param axes The texture axes to align.
 */
void
MeshEntity::MinAlign(TextureAxisSelection axes)
{
   // Implement this by applying MinAlignInt to each specified axis.
   ProcessForAxes(&MeshEntity::MinAlignInt, axes);
}

/**
 * For each of the specified texture axes, shift the highest-valued texture
 * coordinates off of the mesh until an integral texture coordinate (texture
 * boundary) is on the mesh edge.
 *
 * @param axes The texture axes to align.
 */
void
MeshEntity::MaxAlign(TextureAxisSelection axes)
{
   // Implement this by applying MaxAlignInt to each specified axis.
   ProcessForAxes(&MeshEntity::MaxAlignInt, axes);
}

/**
 * For each of the specified texture axes, perform either MinMaxAlignStretch
 * or MinMaxAlignShrink; the chosen operation will be the one with the least
 * absolute change in the value of the texture scale.
 *
 * @param axes The texture axes to align.
 */
void
MeshEntity::MinMaxAlignAutoScale(TextureAxisSelection axes)
{
   // Implement this by applying MinMaxAlignAutoScaleInt to each specified axis.
   ProcessForAxes(&MeshEntity::MinMaxAlignAutoScaleInt, axes);
}

/**
 * For each of the specified texture axes, align a texture boundary to one
 * edge of the mesh, then increase the texture scale to align a texture
 * boundary to the other edge of the mesh as well.
 *
 * @param axes The texture axes to align.
 */
void
MeshEntity::MinMaxAlignStretch(TextureAxisSelection axes)
{
   // Implement this by applying MinMaxAlignStretchInt to each specified axis.
   ProcessForAxes(&MeshEntity::MinMaxAlignStretchInt, axes);
}

/**
 * For each of the specified texture axes, align a texture boundary to one
 * edge of the mesh, then decrease the texture scale to align a texture
 * boundary to the other edge of the mesh as well.
 *
 * @param axes The texture axes to align.
 */
void
MeshEntity::MinMaxAlignShrink(TextureAxisSelection axes)
{
   // Implement this by applying MinMaxAlignShrinkInt to each specified axis.
   ProcessForAxes(&MeshEntity::MinMaxAlignShrinkInt, axes);
}

/**
 * Set the texture scaling along the rows or columns of the mesh. This
 * affects only the texture axis that is naturally associated with rows (S)
 * or columns (T) according to the chosen sliceType.
 * 
 * The scaling may be input either as a multiple of the natural scale that
 * Radiant would choose for this texture, or as the number of tiles of the
 * texture that should fit on the mesh's row/column.
 * 
 * Among the slices perpendicular to the direction of scaling, an alignment
 * slice is used to fix the position of the texture boundary.
 * 
 * A reference slice may optionally be chosen among the slices parallel to
 * the scaling direction. If a reference slice is not specified, then the
 * texture coordinates are independently determined for each slice. If a
 * reference slice is specified, its texture coordinates are calculated first
 * and used to affect the other slices. The reference slice's amount of
 * texture tiling will be re-used for all other slices; optionally, the
 * texture coordinate at each control point within the reference slice can be
 * copied to the corresponding control point in every other slice.
 *
 * @param sliceType           Choose to scale along rows or columns.
 * @param alignSlice          Pointer to alignment slice description; if NULL,
 *                            slice 0 is assumed.
 * @param refSlice            Pointer to reference slice description,
 *                            including how to use the reference; NULL if no
 *                            reference.
 * @param naturalScale        true if naturalScaleOrTiles is a factor of the
 *                            Radiant natural scale; false if
 *                            naturalScaleOrTiles is a number of tiles.
 * @param naturalScaleOrTiles Scaling determinant, interpreted according to
 *                            the naturalScale parameter.
 */
void
MeshEntity::SetScale(SliceType sliceType,
                     const SliceDesignation *alignSlice,
                     const RefSliceDescriptor *refSlice,
                     bool naturalScale,
                     float naturalScaleOrTiles)
{
   // We're about to make changes!
   CreateUndoPoint();

   // Check for bad inputs. Also convert from natural scale to raw scale.
   if (alignSlice != NULL && !alignSlice->maxSlice)
   {
      if (alignSlice->index < 0 ||
          alignSlice->index >= (int)SliceSize(sliceType))
      {
         _errorReportCallback(_errorBadSliceString[OtherSliceType(sliceType)]);
         return;
      }
   }
   if (refSlice != NULL && !refSlice->designation.maxSlice)
   {
      if (refSlice->designation.index < 0 ||
          refSlice->designation.index >= (int)_numSlices[sliceType])
      {
         _errorReportCallback(_errorBadSliceString[sliceType]);
         return;
      }
   }
   TextureAxis axis = _naturalAxis[sliceType];
   float rawScaleOrTiles = naturalScaleOrTiles;
   if (naturalScale)
   {
      // In this case, naturalScaleOrTiles (copied to rawScaleOrTiles) was a
      // natural-scale factor.
      if (rawScaleOrTiles == 0)
      {
         _errorReportCallback(_errorSliceZeroscaleString[sliceType]);
         return;
      }
      // If Radiant's internal orientation is backwards, account for that.
      if (_radiantScaleInverted[sliceType])
      {
         rawScaleOrTiles = -rawScaleOrTiles;
      }
      // Raw scale is the divisor necessary to get texture coordinate from
      // worldspace distance, so we can derive that from the "natural" scale.
      rawScaleOrTiles *= _naturalTexUnits[axis];
   }
   else
   {
      // In this case, naturalScaleOrTiles (copied to rawScaleOrTiles) was a
      // tiling amount.
      if (rawScaleOrTiles == 0)
      {
         // XXX We could try to make zero-tiles work ("infinite scale": all
         // values for this axis are the same along the slice). Need to sort
         // out divide-by-zero dangers down the road.
         _errorReportCallback(_errorSliceZerotilesString[sliceType]);
         return;
      }
      // If Radiant's internal orientation is backwards, account for that.
      if (_radiantTilesInverted[sliceType])
      {
         rawScaleOrTiles = -rawScaleOrTiles;
      }
   }

   // Make sure we have a definite number for the alignment slice, and for the
   // reference slice if any.
   int alignSliceInt =
      InternalSliceDesignation(alignSlice, (SliceType)OtherSliceType(sliceType));
   RefSliceDescriptorInt descriptor;
   RefSliceDescriptorInt *refSliceInt = // will be NULL if no reference
      InternalRefSliceDescriptor(refSlice, sliceType, descriptor);

   // Generate the surface texture coordinates using the mesh control points
   // and the designated scaling/tiling.
   AllocatedMatrix<float> surfaceValues(_meshData.x(), _meshData.y());
   GenScaledDistanceValues(sliceType, alignSliceInt, refSliceInt,
                           naturalScale, rawScaleOrTiles,
                           surfaceValues);

   // Derive the control point values necessary to achieve those surface values.
   GenControlTexFromSurface(axis, surfaceValues);

   // Done!
   CommitChanges();
}

/**
 * Set the mesh's texture coordinates according to a linear combination of
 * factors. This equation can be used to set the texture coordinates at the
 * control points themselves, or to directly set the texture coordinates at
 * the locations on the mesh surface that correspond to each half-patch
 * interval.
 * 
 * An alignment row is used as the zero-point for any calculations of row
 * number or of distance along a column surface when processing the equation.
 * An alignment column is similarly used. (Note that the number identifying
 * the alignment row/column is the real number used to index into the mesh;
 * it's not the modified number as affected by the alignment column/row when
 * processing the equation. We don't want to be stuck in a chicken-and-egg
 * situation.)
 * 
 * Calculations of distance along row/col surface may optionally be affected
 * by a designated reference row/col. The reference row/col can be used as a
 * source of end-to-end distance only, in which case the proportional spacing
 * of the control points within the affected row/col will determine the
 * distance value to be used for each control point. Or, the distance value
 * for every control point in the reference row/col can be copied to each
 * corresponding control point in the other rows/cols.
 *
 * @param sFactors      Factors to determine the S texture coords; NULL if S
 *                      axis unaffected.
 * @param tFactors      Factors to determine the T texture coords; NULL if T
 *                      axis unaffected.
 * @param alignRow      Pointer to zero-point row; if NULL, row 0 is assumed.
 * @param alignCol      Pointer to zero-point column; if NULL, column 0 is
 *                      assumed.
 * @param refRow        Pointer to reference row description, including how
 *                      to use the reference; NULL if no reference.
 * @param refCol        Pointer to reference column description, including
 *                      how to use the reference; NULL if no reference.
 * @param surfaceValues true if calculations are for S/T values on the mesh
 *                      surface; false if calculations are for S/T values at
 *                      the control points.
 */
void
MeshEntity::GeneralFunction(const GeneralFunctionFactors *sFactors,
                            const GeneralFunctionFactors *tFactors,
                            const SliceDesignation *alignRow,
                            const SliceDesignation *alignCol,
                            const RefSliceDescriptor *refRow,
                            const RefSliceDescriptor *refCol,
                            bool surfaceValues)
{
   // We're about to make changes!
   CreateUndoPoint();

   // Make sure we have a definite number for the alignment slices, and for the
   // reference slices if any.
   int alignRowInt = InternalSliceDesignation(alignRow, ROW_SLICE_TYPE);
   int alignColInt = InternalSliceDesignation(alignRow, COL_SLICE_TYPE);
   RefSliceDescriptorInt rowDescriptor;
   RefSliceDescriptorInt *refRowInt = // will be NULL if no row reference
      InternalRefSliceDescriptor(refRow, ROW_SLICE_TYPE, rowDescriptor);
   RefSliceDescriptorInt colDescriptor;
   RefSliceDescriptorInt *refColInt = // will be NULL if no column reference
      InternalRefSliceDescriptor(refCol, COL_SLICE_TYPE, colDescriptor);

   // Get the surface row/col distance values at each half-patch interval, if
   // the input factors care about distances.
   AllocatedMatrix<float> rowDistances(_meshData.x(), _meshData.y());
   AllocatedMatrix<float> colDistances(_meshData.x(), _meshData.y());
   if ((sFactors != NULL && sFactors->rowDistance != 0.0f) ||
       (tFactors != NULL && tFactors->rowDistance != 0.0f))
   {
      GenScaledDistanceValues(ROW_SLICE_TYPE,
                              alignColInt,
                              refRowInt,
                              true,
                              1.0f,
                              rowDistances);
   }
   if ((sFactors != NULL && sFactors->colDistance != 0.0f) ||
       (tFactors != NULL && tFactors->colDistance != 0.0f))
   {
      GenScaledDistanceValues(COL_SLICE_TYPE,
                              alignRowInt,
                              refColInt,
                              true,
                              1.0f,
                              colDistances);
   }

   // Modify the S axis if requested.
   if (sFactors != NULL)
   {
      GeneralFunctionInt(*sFactors, S_TEX_AXIS, alignRowInt, alignColInt, surfaceValues,
                         rowDistances, colDistances);
   }

   // Modify the T axis if requested.
   if (tFactors != NULL)
   {
      GeneralFunctionInt(*tFactors, T_TEX_AXIS, alignRowInt, alignColInt, surfaceValues,
                         rowDistances, colDistances);
   }

   // Done!
   CommitChanges();
}

/**
 * Update the internally stored information for the min and max extent of the
 * mesh on the specified worldspace axis.
 *
 * @param axis The worldspace axis.
 */
void
MeshEntity::UpdatePosMinMax(PositionAxis axis)
{
   // Iterate over all control points to find the min and max values.
   _posMin[axis] = _meshData(0, 0).m_vertex[axis];
   _posMax[axis] = _posMin[axis];
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex++)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex++)
      {
         float current = _meshData(rowIndex, colIndex).m_vertex[axis];
         if (current < _posMin[axis])
         {
            _posMin[axis] = current;
         }
         if (current > _posMax[axis])
         {
            _posMax[axis] = current;
         }
      }
   }
}

/**
 * Update the internally stored information for the min and max extent of the
 * mesh on the specified texture axis.
 *
 * @param axis The texture axis.
 */
void
MeshEntity::UpdateTexMinMax(TextureAxis axis)
{
   // Bail out if no operations have possibly changed these values.
   if (!_texMinMaxDirty[axis])
   {
      return;
   }

   // Iterate over all control points to find the min and max values.
   _texMin[axis] = _meshData(0, 0).m_texcoord[axis];
   _texMax[axis] = _texMin[axis];
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex++)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex++)
      {
         float current = _meshData(rowIndex, colIndex).m_texcoord[axis];
         if (current < _texMin[axis])
         {
            _texMin[axis] = current;
         }
         if (current > _texMax[axis])
         {
            _texMax[axis] = current;
         }
      }
   }

   // See if the min and max are on texture boundaries.
   _texMinAligned[axis] = (floorf(_texMin[axis]) == _texMin[axis]);
   _texMaxAligned[axis] = (floorf(_texMax[axis]) == _texMax[axis]);

   // Values are good until next relevant operation.
   _texMinMaxDirty[axis] = false;
}

/**
 * Interface to the Radiant undo buffer; save the current state of the mesh
 * to allow rollback to this point by an undo operation.
 */
void
MeshEntity::CreateUndoPoint()
{
   GlobalPatchCreator().Patch_undoSave(_mesh);
}

/**
 * Commit the changes to the mesh so that they will be reflected in Radiant.
 */
void
MeshEntity::CommitChanges()
{
   GlobalPatchCreator().Patch_controlPointsChanged(_mesh);
   // Radiant undo-buffer behavior requires this:
   CreateUndoPoint();
}

/**
 * Convert from SliceDesignation to a slice number. Interpret max slice if
 * necessary, and fall back to slice 0 if unspecified.
 *
 * @param sliceDesignation Pointer to slice description; may be NULL.
 * @param sliceType        Slice kind (row or column).
 *
 * @return The slice number.
 */
int
MeshEntity::InternalSliceDesignation(const SliceDesignation *sliceDesignation,
                                     SliceType sliceType)
{
   if (sliceDesignation != NULL)
   {
      // Interpret "max slice" if necessary.
      if (sliceDesignation->maxSlice)
      {
         return _numSlices[sliceType] - 1;
      }
      else
      {
         return sliceDesignation->index;
      }
   }
   else
   {
      // 0 if unspecified.
      return 0;
   }
}

/**
 * Convert from RefSliceDescriptor to RefSliceDescriptorInt. Interpret max
 * slice if necessary. Populate specified RefSliceDescriptorInt if input is
 * non-NULL and return pointer to it; otherwise return NULL.
 *
 * @param refSlice          Pointer to reference slice description; may be
 *                          NULL.
 * @param sliceType         Slice kind (row or column).
 * @param [out] refSliceInt RefSliceDescriptorInt to populate.
 *
 * @return NULL if input RefSliceDescriptor is NULL; else, pointer to populated
 *         RefSliceDescriptorInt.
 */
MeshEntity::RefSliceDescriptorInt *
MeshEntity::InternalRefSliceDescriptor(const RefSliceDescriptor *refSlice,
                                       SliceType sliceType,
                                       RefSliceDescriptorInt& refSliceInt)
{
   if (refSlice != NULL)
   {
      // Preserve totalLengthOnly.
      refSliceInt.totalLengthOnly = refSlice->totalLengthOnly;
      // Convert slice designator to a slice number.
      refSliceInt.index = InternalSliceDesignation(&(refSlice->designation), sliceType);
      return &refSliceInt;
   }
   else
   {
      // NULL if unspecified.
      return NULL;
   }
}

/**
 * Given a slice and a number of times that its texture should tile along it,
 * find the appropriate texture scale. This result is determined by the
 * surface length along the slice.
 *
 * @param sliceType Slice kind (row or column).
 * @param slice     Slice number, among slices of that kind in mesh.
 * @param axis      The texture axis of interest.
 * @param tiles     Number of times the texture tiles.
 *
 * @return The texture scale corresponding to the tiling amount.
 */
float
MeshEntity::GetSliceTexScale(SliceType sliceType,
                             int slice,
                             TextureAxis axis,
                             float tiles)
{
   // XXX Similar to GenScaledDistanceValues; refactor for shared code?

   // We're going to be walking patches along the mesh, choosing the patches
   // that surround/affect the slice we are interested in. We'll calculate
   // the length of the slice's surface across each patch & add those up.
   
   // A SlicePatchContext will contain all the necessary information to
   // evaluate our slice's surface length within each patch. Some aspects of
   // the SlicePatchContext will vary as we move from patch to patch, but we
   // can go ahead and calculate now any stuff that is invariant along the
   // slice's direction.
   SlicePatchContext context;
   context.sliceType = sliceType;
   if (slice != 0)
   {
      // This is the position of the slice within each patch, in the direction
      // orthogonal to the slice. Even-numbered slices are at the edge of the
      // patch (position 1.0), while odd-numbered slices are in the middle
      // (position 0.5).
      context.position = 1.0f - ((float)(slice & 0x1) / 2.0f);
   }
   else
   {
      // For the first slice, we can't give it the usual treatment for even-
      // numbered slices (since there is no patch "before" it), so it gets
      // position 0.0 instead.
      context.position = 0.0f;
   }
   // This is the slice of the same kind that defines the 0.0 edge of the
   // patch. It will be the next lowest even-numbered slice. (Note the
   // integer division here.)
   context.edgeSlice[sliceType] = 2 * ((slice - 1) / 2);

   // Now it's time to walk the patches.

   // We start off with no cumulative distance yet.
   float cumulativeDistance = 0.0f;

   // By iterating over the number of control points in this slice by
   // increments of 2, we'll be walking the slice in patch-sized steps. Since
   // we are only interested in the total length, we don't need to check in at
   // finer granularities.
   for (unsigned halfPatch = 2; halfPatch < SliceSize(sliceType); halfPatch += 2)
   {
      // Find the slice-of-other-kind that defines the patch edge orthogonal
      // to our slice.
      context.edgeSlice[OtherSliceType(sliceType)] = 2 * ((halfPatch - 1) / 2);
      // Estimate the slice length along the surface of the patch.
      float segmentLengthEstimate = EstimateSegmentLength(0.0f, 1.0f, context);
      // Recursively refine that estimate until it is good enough, then add it
      // to our cumulative distance.
      cumulativeDistance += RefineSegmentLength(0.0f, 1.0f, context,
                                                segmentLengthEstimate,
                                                UNITS_ERROR_BOUND);
   }

   // The scale along this slice is defined as the surface length divided by
   // the "natural" number of texture units along that length.
   return cumulativeDistance / (tiles * _naturalTexUnits[axis]);
}

/**
 * Populate the SliceTexInfo for the indicated slice and texture axis.
 *
 * @param sliceType  Slice kind (row or column).
 * @param slice      Slice number, among slices of that kind in mesh.
 * @param axis       The texture axis of interest.
 * @param [out] info Information on scale, tiles, and min/max for the
 *                   specified texture axis.
 *
 * @return true on success, false if slice cannot be processed.
 */
bool
MeshEntity::GetSliceTexInfo(SliceType sliceType,
                            int slice,
                            TextureAxis axis,
                            SliceTexInfo& info)
{
   // Bail out now if slice # is bad.
   if (slice < 0 ||
       slice >= (int)_numSlices[sliceType])
   {
      _errorReportCallback(_errorBadSliceString[sliceType]);
      return false;
   }

   // Calculate the # of times the texture is tiled along the specified axis
   // on this slice, and find the min and max values for that axis.
   float texBegin =
      MatrixElement(_meshData, sliceType, slice, 0).m_texcoord[axis];
   float texEnd =
      MatrixElement(_meshData, sliceType, slice, SliceSize(sliceType) - 1).m_texcoord[axis];
   info.tiles = texEnd - texBegin;
   if (texBegin < texEnd)
   {
      info.min = texBegin;
      info.max = texEnd;
   }
   else
   {
      info.min = texEnd;
      info.max = texBegin;
   }

   // Calculate the texture scale along this slice, using the tiling info
   // along with the texture size and the length of the slice's surface.
   info.scale = GetSliceTexScale(sliceType, slice, axis, info.tiles);
   return true;
}

/**
 * Take the information from GetSliceTexInfo and sanitize it for reporting.
 * Optionally print to a provided message buffer and/or supply data to a
 * provided TexInfoCallback.
 *
 * @param sliceType         Slice kind (row or column).
 * @param slice             Slice number, among slices of that kind in mesh.
 * @param axis              The texture axis of interest.
 * @param messageBuffer     Buffer for message data; NULL if none.
 * @param messageBufferSize Size of the message buffer.
 * @param texInfoCallback   Callback for passing texture scale/tiles info;
 *                          NULL if none.
 */
void
MeshEntity::ReportSliceTexInfo(SliceType sliceType,
                               int slice,
                               TextureAxis axis,
                               char *messageBuffer,
                               unsigned messageBufferSize,
                               const TexInfoCallback *texInfoCallback)
{
   // Fetch the raw info.
   SliceTexInfo info;
   if (!GetSliceTexInfo(sliceType, slice, axis, info))
   {
      return;
   }

   // Account for Radiant-inverted.
   if (_radiantScaleInverted[sliceType])
   {
      info.scale = -info.scale;
   }
   if (_radiantTilesInverted[sliceType])
   {
      info.tiles = -info.tiles;
   }

   // Send texture info to callback if one is provided.
   bool infscale = (info.scale > FLT_MAX || info.scale < -FLT_MAX);
   if (texInfoCallback != NULL)
   {
      if (!infscale)
      {
         (*texInfoCallback)(SanitizeFloat(info.scale), SanitizeFloat(info.tiles));
      }
      else
      {
         // "Infinite scale" prevents us from invoking the callback, so
         // raise a warning about that.
         _warningReportCallback(_warningSliceInfscaleFormatString[sliceType]);
      }
   }

   // Write texture info to buffer if one is provided.
   if (messageBuffer != NULL)
   {
      if (!infscale)
      {
         snprintf(messageBuffer, messageBufferSize,
            _infoSliceFormatString[sliceType], slice,
            SanitizeFloat(info.scale), SanitizeFloat(info.tiles),
            SanitizeFloat(info.min), SanitizeFloat(info.max));
      }
      else
      {
         // Special handling for "infinite scale".
         snprintf(messageBuffer, messageBufferSize,
            _infoSliceInfscaleFormatString[sliceType], slice,
            SanitizeFloat(info.tiles),
            SanitizeFloat(info.min), SanitizeFloat(info.max));
      }
   }
}

/**
 * Apply some function with the InternalImpl signature to each of the
 * designated texture axes. The undo point and state commit operations
 * are handled here for such functions.
 *
 * @param internalImpl The function to apply.
 * @param axes         The texture axes to affect.
 */
void
MeshEntity::ProcessForAxes(InternalImpl internalImpl,
                           TextureAxisSelection axes)
{
   // We're about to make changes!
   CreateUndoPoint();

   // Apply the function to the requested axes.
   bool sChanged = false;
   bool tChanged = false;
   if (axes != T_TEX_AXIS_ONLY)
   {
      sChanged = (*this.*internalImpl)(S_TEX_AXIS);
   }
   if (axes != S_TEX_AXIS_ONLY)
   {
      tChanged = (*this.*internalImpl)(T_TEX_AXIS);
   }

   // Done! Commit changes if necessary.
   if (sChanged || tChanged)
   {
      CommitChanges();
   }
}

/**
 * Add an offset to all control point values for the given texture axis.
 *
 * @param axis  The texture axis to affect.
 * @param shift The offset to add.
 */
void
MeshEntity::Shift(TextureAxis axis,
                  float shift)
{
   // Iterate over all control points and add the offset.
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex++)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex++)
      {
         _meshData(rowIndex, colIndex).m_texcoord[axis] += shift;
      }
   }

   // This operation might have changed texture min/max.
   _texMinMaxDirty[axis] = true;
}

/**
 * On the given texture axis, find the distance of all control point values
 * from the current minimum value and multiply that distance by the given
 * scale factor.
 *
 * @param axis  The texture axis to affect.
 * @param scale The scale factor.
 */
void
MeshEntity::Scale(TextureAxis axis,
                  float scale)
{
   // Make sure the min value is updated; we'll need it below.
   UpdateTexMinMax(axis);

   // Iterate over all control points and apply the scale factor.
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex++)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex++)
      {
         // Leave the current minimum edge in place and scale out from that.
         _meshData(rowIndex, colIndex).m_texcoord[axis] =
            ((_meshData(rowIndex, colIndex).m_texcoord[axis] - _texMin[axis]) * scale) +
            _texMin[axis];
      }
   }

   // This operation might have changed texture min/max.
   _texMinMaxDirty[axis] = true;
}

/**
 * Implementation of MinAlign for a single texture axis.
 *
 * @param axis The texture axis to affect.
 *
 * @return true if the mesh was changed, false if not.
 */
bool
MeshEntity::MinAlignInt(TextureAxis axis)
{
   // Make sure the min-aligned value is updated.
   UpdateTexMinMax(axis);

   // If already aligned, we're done.
   if (_texMinAligned[axis])
   {
      // Didn't make changes.
      return false;
   }

   // Otherwise shift by the necessary amount to align.
   Shift(axis, ceilf(_texMin[axis]) - _texMin[axis]);

   // Made changes.
   return true;
}

/**
 * Implementation of MaxAlign for a single texture axis.
 *
 * @param axis The texture axis to affect.
 *
 * @return true if the mesh was changed, false if not.
 */
bool
MeshEntity::MaxAlignInt(TextureAxis axis)
{
   // Make sure the max-aligned value is updated.
   UpdateTexMinMax(axis);

   // If already aligned, we're done.
   if (_texMaxAligned[axis])
   {
      // Didn't make changes.
      return false;
   }

   // Otherwise shift by the necessary amount to align.
   Shift(axis, ceilf(_texMax[axis]) - _texMax[axis]);

   // Made changes.
   return true;
}

/**
 * Implementation of MinMaxAlignAutoScale for a single texture axis.
 *
 * @param axis The texture axis to affect.
 *
 * @return true if the mesh was changed, false if not.
 */
bool
MeshEntity::MinMaxAlignAutoScaleInt(TextureAxis axis)
{
   // Make sure the max value is updated.
   UpdateTexMinMax(axis);

   // Choose to stretch or shrink, based on which will cause less change.
   if ((_texMax[axis] - floorf(_texMax[axis])) < 0.5)
   {
      return MinMaxAlignStretchInt(axis);
   }
   else
   {
      return MinMaxAlignShrinkInt(axis);
   }
}

/**
 * The meat of MinMaxAlignStretchInt and MinMaxAlignShrinkInt.
 *
 * @param axis The texture axis to affect.
 * @param op   Whether to stretch or shrink.
 *
 * @return true if the mesh was changed, false if not.
 */
bool
MeshEntity::MinMaxAlignScale(TextureAxis axis,
                             ScaleOperation op)
{
   // First make sure we are min-aligned.
   bool changed = MinAlignInt(axis);

   // Make sure the min/max values are updated.
   UpdateTexMinMax(axis);

   // More work to do if not max-aligned.
   if (!_texMaxAligned[axis])
   {
      // Find the current tiling.
      float oldRepeats = _texMax[axis] - _texMin[axis];
      // Find the desired tiling, depending on whether we are stretching or
      // shrinking.
      float newRepeats;
      if (op == STRETCH_SCALE_OP)
      {
         newRepeats = floorf(_texMax[axis]) - _texMin[axis];
      }
      else
      {
         newRepeats = ceilf(_texMax[axis]) - _texMin[axis];
      }
      // Apply the necessary scaling to get the desired tiling.
      Scale(axis, newRepeats / oldRepeats);
      // Made changes.
      changed = true;
   }

   return changed;
}

/**
 * Implementation of MinMaxAlignStretch for a single texture axis.
 *
 * @param axis The texture axis to affect.
 *
 * @return true if the mesh was changed, false if not.
 */
bool
MeshEntity::MinMaxAlignStretchInt(TextureAxis axis)
{
   // Hand off to MinMaxAlignScale.
   return MinMaxAlignScale(axis, STRETCH_SCALE_OP);
}

/**
 * Implementation of MinMaxAlignShrink for a single texture axis.
 *
 * @param axis The texture axis to affect.
 *
 * @return true if the mesh was changed, false if not.
 */
bool
MeshEntity::MinMaxAlignShrinkInt(TextureAxis axis)
{
   // Hand off to MinMaxAlignScale.
   return MinMaxAlignScale(axis, SHRINK_SCALE_OP);
}

/**
 * Calculate the d(x, y, or z)/dt of a patch slice, evaluated at a given t
 * (parameter for the Bezier function, between 0 and 1).
 *
 * @param axis    The worldspace axis of interest.
 * @param t       Bezier parameter.
 * @param context The slice and patch.
 *
 * @return d(x, y, or z)/dt at the given t.
 */
float
MeshEntity::SliceParametricSpeedComponent(PositionAxis axis,
                                          float t,
                                          const SlicePatchContext& context)
{
   float a = 1.0f - context.position;
   float b = 2.0f * context.position * a;
   a *= a;
   float c = context.position * context.position;
   float d = 2.0f * t - 2.0f;
   float e = 2.0f - 4.0f * t;
   float f = 2.0f * t;
   int patchStartCol = context.edgeSlice[COL_SLICE_TYPE];
   int patchStartRow = context.edgeSlice[ROW_SLICE_TYPE];

   if (context.sliceType == ROW_SLICE_TYPE)
   {
      return
         _meshData(patchStartRow+0, patchStartCol+0).m_vertex[axis] * a * d +
         _meshData(patchStartRow+0, patchStartCol+1).m_vertex[axis] * a * e +
         _meshData(patchStartRow+0, patchStartCol+2).m_vertex[axis] * a * f +
         _meshData(patchStartRow+1, patchStartCol+0).m_vertex[axis] * b * d +
         _meshData(patchStartRow+1, patchStartCol+1).m_vertex[axis] * b * e +
         _meshData(patchStartRow+1, patchStartCol+2).m_vertex[axis] * b * f +
         _meshData(patchStartRow+2, patchStartCol+0).m_vertex[axis] * c * d +
         _meshData(patchStartRow+2, patchStartCol+1).m_vertex[axis] * c * e +
         _meshData(patchStartRow+2, patchStartCol+2).m_vertex[axis] * c * f;
   }
   else
   {
      return
         _meshData(patchStartRow+0, patchStartCol+0).m_vertex[axis] * a * d +
         _meshData(patchStartRow+1, patchStartCol+0).m_vertex[axis] * a * e +
         _meshData(patchStartRow+2, patchStartCol+0).m_vertex[axis] * a * f +
         _meshData(patchStartRow+0, patchStartCol+1).m_vertex[axis] * b * d +
         _meshData(patchStartRow+1, patchStartCol+1).m_vertex[axis] * b * e +
         _meshData(patchStartRow+2, patchStartCol+1).m_vertex[axis] * b * f +
         _meshData(patchStartRow+0, patchStartCol+2).m_vertex[axis] * c * d +
         _meshData(patchStartRow+1, patchStartCol+2).m_vertex[axis] * c * e +
         _meshData(patchStartRow+2, patchStartCol+2).m_vertex[axis] * c * f;
   }
}

/**
 * Calculates the rate of change in worldspace units of a patch slice,
 * evaluated at a given t (parameter for the Bezier function, between 0 and
 * 1).
 *
 * @param t       Bezier parameter.
 * @param context The slice and patch.
 *
 * @return Path length.
 */
float
MeshEntity::SliceParametricSpeed(float t,
                                 const SlicePatchContext& context)
{
   float xDotEval = SliceParametricSpeedComponent(X_POS_AXIS, t, context);
   float yDotEval = SliceParametricSpeedComponent(Y_POS_AXIS, t, context);
   float zDotEval = SliceParametricSpeedComponent(Z_POS_AXIS, t, context);
   return sqrtf(xDotEval*xDotEval + yDotEval*yDotEval + zDotEval*zDotEval);
}

/**
 * Estimate the surface length of a slice segment, using ten point
 * Gauss-Legendre integration of the parametric speed function. The value
 * returned will always be positive (absolute value).
 *
 * @param startPosition Bezier parameter value for the start point of the
 *                      slice segment.
 * @param endPosition   Bezier parameter value for the end point of the slice
 *                      segment.
 * @param context       The slice and patch.
 *
 * @return Estimate of segment length.
 */
float
MeshEntity::EstimateSegmentLength(float startPosition,
                                  float endPosition,
                                  const SlicePatchContext& context)
{
   // Gauss-Legendre implementation taken from "Numerical Recipes in C".

   static float x[] = {0.0f, 0.1488743389f, 0.4333953941f, 0.6794095682f, 0.8650633666f, 0.9739065285f};
   static float w[] = {0.0f, 0.2955242247f, 0.2692667193f, 0.2190863625f, 0.1494513491f, 0.0666713443f};

   float xm = 0.5f * (endPosition + startPosition);
   float xr = 0.5f * (endPosition - startPosition);
   float s = 0.0f;

   for (unsigned j = 1; j <= 5; j++) {
      float dx = xr * x[j];
      s += w[j] * (SliceParametricSpeed(xm + dx, context) +
                   SliceParametricSpeed(xm - dx, context));
   }

   return fabsf(s * xr);
}

/**
 * Recursively improve the estimate of the surface length of a slice segment,
 * by estimating the length of its halves, until the change between estimates
 * is equal to or less than an acceptable error threshold.
 *
 * @param startPosition         Bezier parameter value for the start point of
 *                              the slice segment.
 * @param endPosition           Bezier parameter value for the end point of the
 *                              slice segment.
 * @param context               The slice and patch.
 * @param segmentLengthEstimate Starting estimate for segment legnth.
 * @param maxError              Max acceptable variance between estimates.
 *
 * @return Improved estimate of segment length.
 */
float
MeshEntity::RefineSegmentLength(float startPosition,
                                float endPosition,
                                const SlicePatchContext& context,
                                float segmentLengthEstimate,
                                float maxError)
{
   // Estimate the lengths of the two halves of this segment.
   float midPosition = (startPosition + endPosition) / 2.0f;
   float leftLength = EstimateSegmentLength(startPosition, midPosition, context);
   float rightLength = EstimateSegmentLength(midPosition, endPosition, context);

   // If the sum of the half-segment estimates is too far off from the
   // whole-segment estimate, then we're in a regime with too much error in
   // the estimates. Recurse to refine the half-segment estimates.
   if (fabsf(segmentLengthEstimate - (leftLength + rightLength)) > maxError)
   {
      leftLength = RefineSegmentLength(startPosition, midPosition,
                                       context,
                                       leftLength,
                                       maxError / 2.0f);
      rightLength = RefineSegmentLength(midPosition, endPosition,
                                        context,
                                        rightLength,
                                        maxError / 2.0f);
   }

   // Return the sum of the (refined) half-segment estimates.
   return (leftLength + rightLength);
}

/**
 * Derive control point texture coordinates (on a given texture axis) from a
 * set of mesh surface texture coordinates.
 *
 * @param axis          The texture axis of interest.
 * @param surfaceValues The surface texture coordinates.
 */
void
MeshEntity::GenControlTexFromSurface(TextureAxis axis,
                                     const Matrix<float>& surfaceValues)
{
   // The control points on even rows & even columns (i.e. patch corners)
   // have texture coordinates that match the surface values.
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex += 2)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex += 2)
      {
         _meshData(rowIndex, colIndex).m_texcoord[axis] =
            surfaceValues(rowIndex, colIndex);
      }
   }

   // Set the control points on odd rows & even columns (i.e. the centers of
   // columns that are patch edges).
   for (unsigned rowIndex = 1; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex += 2)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex += 2)
      {
         _meshData(rowIndex, colIndex).m_texcoord[axis] =
            2.0f * surfaceValues(rowIndex, colIndex) -
            (surfaceValues(rowIndex - 1, colIndex) +
             surfaceValues(rowIndex + 1, colIndex)) / 2.0f;
      }
   }

   // Set the control points on even rows & odd columns (i.e. the centers of
   // rows that are patch edges).
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex += 2)
   {
      for (unsigned colIndex = 1; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex += 2)
      {
         _meshData(rowIndex, colIndex).m_texcoord[axis] =
            2.0f * surfaceValues(rowIndex, colIndex) -
            (surfaceValues(rowIndex, colIndex - 1) +
             surfaceValues(rowIndex, colIndex + 1)) / 2.0f;
      }
   }

   // And finally on odd rows & odd columns (i.e. patch centers).
   for (unsigned rowIndex = 1; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex += 2)
   {
      for (unsigned colIndex = 1; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex += 2)
      {
         _meshData(rowIndex, colIndex).m_texcoord[axis] =
            4.0f * surfaceValues(rowIndex, colIndex) -
            (surfaceValues(rowIndex, colIndex - 1) +
             surfaceValues(rowIndex, colIndex + 1) +
             surfaceValues(rowIndex - 1, colIndex) +
             surfaceValues(rowIndex + 1, colIndex)) / 2.0f -
            (surfaceValues(rowIndex - 1, colIndex - 1) +
             surfaceValues(rowIndex + 1, colIndex + 1) +
             surfaceValues(rowIndex - 1, colIndex + 1) +
             surfaceValues(rowIndex + 1, colIndex - 1)) / 4.0f;
      }
   }

   // This operation might have changed texture min/max.
   _texMinMaxDirty[axis] = true;
}

/**
 * Overwrite control point texture coordinates (on a given texture axis) with
 * the input texture coordinates.
 *
 * @param axis   The texture axis of interest.
 * @param values The input texture coordinates.
 */
void
MeshEntity::CopyControlTexFromValues(TextureAxis axis,
                                     const Matrix<float>& values)
{
   // Iterate over all control points and just do a straight copy.
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex++)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex++)
      {
         _meshData(rowIndex, colIndex).m_texcoord[axis] =
            values(rowIndex, colIndex);
      }
   }

   // This operation might have changed texture min/max.
   _texMinMaxDirty[axis] = true;
}

/**
 * Derive a set of surface texture coordinates (on a given texture axis) from
 * the control point texture coordinates.
 *
 * @param axis                The texture axis of interest.
 * @param [out] surfaceValues The surface texture coordinates.
 */
void
MeshEntity::GenSurfaceFromControlTex(TextureAxis axis,
                                     Matrix<float>& surfaceValues)
{
   // The surface values on even rows & even columns (i.e. patch corners)
   // have texture coordinates that match the control points.
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex += 2)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex += 2)
      {
         surfaceValues(rowIndex, colIndex) =
            _meshData(rowIndex, colIndex).m_texcoord[axis];
      }
   }

   // Set the surface values on odd rows & odd columns (i.e. patch centers).
   for (unsigned rowIndex = 1; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex += 2)
   {
      for (unsigned colIndex = 1; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex += 2)
      {
         surfaceValues(rowIndex, colIndex) =
            _meshData(rowIndex, colIndex).m_texcoord[axis] / 4.0f +
            (_meshData(rowIndex, colIndex - 1).m_texcoord[axis] +
             _meshData(rowIndex, colIndex + 1).m_texcoord[axis] +
             _meshData(rowIndex - 1, colIndex).m_texcoord[axis] +
             _meshData(rowIndex + 1, colIndex).m_texcoord[axis]) / 8.0f +
            (_meshData(rowIndex - 1, colIndex - 1).m_texcoord[axis] +
             _meshData(rowIndex + 1, colIndex + 1).m_texcoord[axis] +
             _meshData(rowIndex - 1, colIndex + 1).m_texcoord[axis] +
             _meshData(rowIndex + 1, colIndex - 1).m_texcoord[axis]) / 16.0f;
      }
   }

   // Set the surface values on even rows & odd columns (i.e. the centers of
   // rows that are patch edges).
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex += 2)
   {
      for (unsigned colIndex = 1; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex += 2)
      {
         surfaceValues(rowIndex, colIndex) =
            _meshData(rowIndex, colIndex).m_texcoord[axis] / 2.0f +
            (_meshData(rowIndex, colIndex - 1).m_texcoord[axis] +
             _meshData(rowIndex, colIndex + 1).m_texcoord[axis]) / 4.0f;
      }
   }

   // And finally on odd rows & even columns (i.e. the centers of columns that
   // are patch edges).
   for (unsigned rowIndex = 1; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex += 2)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex += 2)
      {
         surfaceValues(rowIndex, colIndex) =
            _meshData(rowIndex, colIndex).m_texcoord[axis] / 2.0f +
            (_meshData(rowIndex - 1, colIndex).m_texcoord[axis] +
             _meshData(rowIndex + 1, colIndex).m_texcoord[axis]) / 4.0f;
      }
   }
}

/**
 * Copy the control point texture coordinates (on a given texture axis) to
 * the output texture coordinates parameter.
 *
 * @param axis         The texture axis of interest.
 * @param [out] values The output texture coordinates.
 */
void
MeshEntity::CopyValuesFromControlTex(TextureAxis axis,
                                     Matrix<float>& values)
{
   // Iterate over all control points and just do a straight copy.
   for (unsigned rowIndex = 0; rowIndex < _numSlices[ROW_SLICE_TYPE]; rowIndex++)
   {
      for (unsigned colIndex = 0; colIndex < _numSlices[COL_SLICE_TYPE]; colIndex++)
      {
         values(rowIndex, colIndex) =
            _meshData(rowIndex, colIndex).m_texcoord[axis];
      }
   }
}

/**
 * Generate a set of values based on surface slice lengths and some amount of
 * desired scaling or tiling.
 * 
 * This method does a great deal of the work for the SetScale public method;
 * refer to that method's comment header for more details about the alignment
 * slice and reference slice inputs. The main difference from the SetScale
 * input parameters is that the scale/tiles factor has been processed some.
 * It has been flipped if necessary to account for Radiant's internal
 * scale/tiles orientation differing from the sensible external orientation.
 * And natural scaling has been converted to raw scaling, which is the actual
 * desired divisor to get texture coordinates from worldspace lengths.
 *
 * @param sliceType       Process rows or colums.
 * @param alignSlice      Pointer to alignment slice description; if NULL,
 *                        slice 0 is assumed.
 * @param refSlice        Pointer to reference slice description, including how
 *                        to use the reference; NULL if no reference.
 * @param rawScale        true if rawScaleOrTiles is a scale factor; false if
 *                        rawScaleOrTiles is a number of tiles.
 * @param rawScaleOrTiles Scaling determinant, interpreted according to the
 *                        rawScale parameter.
 * @param [out] values    The generated values.
 */
void
MeshEntity::GenScaledDistanceValues(SliceType sliceType,
                                    int alignSlice,
                                    const RefSliceDescriptorInt *refSlice,
                                    bool rawScale,
                                    float rawScaleOrTiles,
                                    Matrix<float>& values)
{
   // For every half-patch interval along the surface, we want to generate a
   // value based on the surface distance along the row (or column) to that
   // spot. So, first we need to determine those distances.

   // XXX Similar to GetSliceTexScale; refactor for shared code?

   // We're going to be walking patches along the mesh, choosing the patches
   // that surround/affect the slice we are interested in.
   
   // A SlicePatchContext will contain all the necessary information to
   // evaluate our slice's surface length within each patch.
   SlicePatchContext context;
   context.sliceType = sliceType;

   // Pick the slices that we will measure.
   int firstSlice, lastSlice;
   if (refSlice != NULL && !refSlice->totalLengthOnly)
   {
      // If a reference slice is provided, and totalLengthOnly is false, then we
      // will only need to measure the reference slice. The values generated for
      // it will later be copied to other slices.
      firstSlice = lastSlice = refSlice->index;
   }
   else
   {
      // Otherwise we'll measure all of the slices.
      firstSlice = 0;
      lastSlice = _numSlices[sliceType] - 1;
   }

   // Iterate over the slices that need to be measured.
   for (int slice = firstSlice; slice <= lastSlice; slice++)
   {
      // Some aspects of the SlicePatchContext will vary as we move from patch
      // to patch, but we can go ahead and calculate now any stuff that is
      // invariant along the slice's direction.
      if (slice != 0)
      {
         // This is the position of the slice within each patch, in the
         // direction orthogonal to the slice. Even-numbered slices are at the
         // edge of the patch (position 1.0), while odd-numbered slices are in
         // the middle (position 0.5).
         context.position = 1.0f - ((float)(slice & 0x1) / 2.0f);
      }
      else
      {
         // For the first slice, we can't give it the usual treatment for even-
         // numbered slices (since there is no patch "before" it), so it gets
         // position 0.0 instead.
         context.position = 0.0f;
      }
      // This is the slice of the same kind that defines the 0.0 edge of the
      // patch. It will be the next lowest even-numbered slice. (Note the
      // integer division here.)
      context.edgeSlice[sliceType] = 2 * ((slice - 1) / 2);

      // The alignment slice marks the zero-point from which we will be
      // calculating the distances. So the cumulative distance there is zero.
      MatrixElement(values, sliceType, slice, alignSlice) = 0.0f;

      // Now we're going to calculate distances for control points "greater
      // than" the one marked by the alignment slice.

      // Start with zero cumulative distance.
      float cumulativeDistance = 0.0f;
      // Each pair of control points delineates a "half patch" (the middle
      // control point corresponds to surface coords generated from t=0.5).
      // Since distance measurements are done within each patch, and we want to
      // measure the distance at half-patch increments, we need to alternate
      // doing the segment measurements from 0 to 0.5 and from 0.5 to 1.0 (for
      // values of t). We start with a t target of 0.5 or 1.0 depending on the
      // even/odd nature of the alignment slice.
      float slicewiseFraction = (float)((alignSlice & 0x1) + 1) / 2.0f;
      // Iterate over the control points greater than the alignment point.
      for (int halfPatch = alignSlice + 1; halfPatch < (int)SliceSize(sliceType); halfPatch++)
      {
         // Find the slice-of-other-kind that defines the patch edge orthogonal
         // to our slice.
         context.edgeSlice[OtherSliceType(sliceType)] = 2 * ((halfPatch - 1) / 2);
         // Estimate the slice length along the surface of the half patch.
         float segmentLengthEstimate =
            EstimateSegmentLength(slicewiseFraction - 0.5f, slicewiseFraction, context);
         // Recursively refine that estimate until it is good enough, then add it
         // to our cumulative distance.
         cumulativeDistance +=
            RefineSegmentLength(slicewiseFraction - 0.5f, slicewiseFraction, context,
                                segmentLengthEstimate, UNITS_ERROR_BOUND);
         // Store that cumulative distance in the output array.
         MatrixElement(values, sliceType, slice, halfPatch) = cumulativeDistance;
         // Flip to measure the other half patch.
         slicewiseFraction = 1.5f - slicewiseFraction;
      }

      // Now we're going to calculate distances for control points "less
      // than" the one marked by the alignment slice.

      // Start with zero cumulative distance.
      cumulativeDistance = 0.0f;
      // We need to alternate doing the segment measurements from 1.0 to 0.5 and
      // from 0.5 to 0 (for values of t). We start with a t target of 0.5 or 0
      // depending on the even/odd nature of the alignment slice.
      slicewiseFraction = (float)((alignSlice - 1) & 0x1) / 2.0f;
      // Iterate over the control points less than the alignment point.
      for (int halfPatch = alignSlice - 1; halfPatch >= 0; halfPatch--)
      {
         // Find the slice-of-other-kind that defines the patch edge orthogonal
         // to our slice.
         context.edgeSlice[OtherSliceType(sliceType)] = 2 * ((halfPatch - 1) / 2);
         // Estimate the slice length along the surface of the half patch.
         float segmentLengthEstimate =
            EstimateSegmentLength(slicewiseFraction + 0.5f, slicewiseFraction, context);
         // Recursively refine that estimate until it is good enough, then add it
         // to our cumulative distance. (Which is negative on this side!)
         cumulativeDistance -=
            RefineSegmentLength(slicewiseFraction + 0.5f, slicewiseFraction, context,
                                segmentLengthEstimate, UNITS_ERROR_BOUND);
         // Store that cumulative distance in the output array.
         MatrixElement(values, sliceType, slice, halfPatch) = cumulativeDistance;
         // Flip to measure the other half patch.
         slicewiseFraction = 0.5f - slicewiseFraction;
      }
   }

   // Now we may adjust the distance values based on scaling/tiling input.
 
   // If there's a reference slice, we're going to need to know the total slice
   // length, so save that away.
   float refTotalLength;
   if (refSlice != NULL)
   {
      refTotalLength =
         MatrixElement(values, sliceType, refSlice->index, SliceSize(sliceType) - 1) -
         MatrixElement(values, sliceType, refSlice->index, 0);
   }
   else
   {
      refTotalLength = 1.0f; // unused, but avoid uninitialized-var warning
   }

#if defined(_DEBUG)
    ASSERT_MESSAGE(refTotalLength != 0.0f, "calculated length of reference slice is zero");
    ASSERT_MESSAGE(rawScaleOrTiles != 0.0f, "internal scale or tiles value is zero");
#endif

   // Iterate over the slices we're processing and adjust the distance values
   // (remember that this may just be the reference slice).
   for (int slice = firstSlice; slice <= lastSlice; slice++)
   {
      // Figure out what we're going to divide the distances by.
      float scaleFactor;
      if (rawScale)
      {
         // In this case we've just been passed in the value to divide by.
         scaleFactor = rawScaleOrTiles;
         if (refSlice != NULL)
         {
            // However if there's a reference slice, adjust the divisor by the
            // ratio of this slice's length to the reference slice's length.
            // (Which is a NOP if this slice actually is the reference slice.)
            // Example: if the ref slice is length 2, this slice is length 3,
            // and the raw scale factor is 4... then all distances on the ref
            // slice would be divided by 4 * 2 / 2 = 4, and distances on this
            // slice would be divided by 4 * 3 / 2 = 6.
            scaleFactor *=
               ((MatrixElement(values, sliceType, slice, SliceSize(sliceType) - 1) -
                 MatrixElement(values, sliceType, slice, 0)) /
                refTotalLength);
         }
      }
      else
      {
         // In this case we've been passed in a desired tiling value. We're
         // going to want to eventually divide the distances by length / tiles.
         // Example: if this slice is length 6 and the desired tiling is 3, we
         // will divide the distances on this slice by 6 / 3 = 2.
         scaleFactor =
            (MatrixElement(values, sliceType, slice, SliceSize(sliceType) - 1) -
             MatrixElement(values, sliceType, slice, 0)) /
            rawScaleOrTiles;
      }
      // Adjust the distances for this slice by the divisor we calculated.
      for (unsigned halfPatch = 0; halfPatch < SliceSize(sliceType); halfPatch++)
      {
         MatrixElement(values, sliceType, slice, halfPatch) /= scaleFactor;
      }
   }

   // One final step if we have a reference slice and totalLengthOnly is false.
   // In that case, up until this point we have only been processing the
   // reference slice. Now we have to copy the reference slice's values to all
   // other slices.
   // (These loops also copy the reference slice to itself, which is fine.)
   if (refSlice != NULL && !refSlice->totalLengthOnly)
   {
      for (unsigned slice = 0; slice < _numSlices[sliceType]; slice++)
      {
         for (unsigned halfPatch = 0; halfPatch < SliceSize(sliceType); halfPatch++)
         {
            MatrixElement(values, sliceType, slice, halfPatch) =
               MatrixElement(values, sliceType, refSlice->index, halfPatch);
         }
      }
   }
}

/**
 * Generate coordinates for a specified texture axis based on a linear
 * combination of factors. This method does the final work for the
 * GeneralFunction public method.
 *
 * @param factors       Factors to determine the texture coords.
 * @param axis          The texture axis to process.
 * @param alignRow      Zero-point row.
 * @param alignCol      Zero-point column.
 * @param surfaceValues true if calculations are for S/T values on the mesh
 *                      surface; false if calculations are for S/T values at
 *                      the control points.
 * @param rowDistances  Surface distance-along-row values (measured from
 *                      alignment column) for spots corresponding to each
 *                      control point.
 * @param colDistances  Surface distance-along-column values (measured from
 *                      alignment row) for spots corresponding to each
 *                      control point.
 */
void
MeshEntity::GeneralFunctionInt(const GeneralFunctionFactors& factors,
                               TextureAxis axis,
                               int alignRow,
                               int alignCol,
                               bool surfaceValues,
                               const Matrix<float>& rowDistances,
                               const Matrix<float>& colDistances)
{
   // Grab the "original value" info if the equation uses it.
   AllocatedMatrix<float> oldValues(_meshData.x(), _meshData.y());
   AllocatedMatrix<float> newValues(_meshData.x(), _meshData.y());
   if (factors.oldValue != 0.0f)
   {
      if (surfaceValues)
      {
         // Will be manipulating surface values.
         GenSurfaceFromControlTex(axis, oldValues);
      }
      else
      {
         // Will be manipulating control point values.
         CopyValuesFromControlTex(axis, oldValues);
      }
   }

   // Iterate over all values and apply the equation.
   for (int rowIndex = 0; rowIndex < (int)_numSlices[ROW_SLICE_TYPE]; rowIndex++)
   {
      for (int colIndex = 0; colIndex < (int)_numSlices[COL_SLICE_TYPE]; colIndex++)
      {
         newValues(rowIndex, colIndex) =
            factors.oldValue * oldValues(rowIndex, colIndex) +
            factors.rowDistance * rowDistances(rowIndex, colIndex) +
            factors.colDistance * colDistances(rowIndex, colIndex) +
            factors.rowNumber * (rowIndex - alignRow) +
            factors.colNumber * (colIndex - alignCol) +
            factors.constant;
      }
   }

   // Store the generated values.
   if (surfaceValues)
   {
      // If we're manipulating surface values, figure the necessary control
      // point values to make those.
      GenControlTexFromSurface(axis, newValues);
   }
   else
   {
      // If we're manipulating control point values, store the new values.
      CopyControlTexFromValues(axis, newValues);
   }
}