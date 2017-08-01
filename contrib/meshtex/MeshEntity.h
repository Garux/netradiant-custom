/**
 * @file MeshEntity.h
 * Declares the MeshEntity class.
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

#if !defined(INCLUDED_MESHENTITY_H)
#define INCLUDED_MESHENTITY_H

#include "AllocatedMatrix.h"

#include "scenelib.h"
#include "ipatch.h"
#include "generic/callback.h"

/**
 * Wrapper for a biquadratic Bezier patch mesh entity from Radiant.
 * Instantiate this for a given patch mesh, then use the methods to
 * interrogate or modify the mesh. It is intended that this object be
 * instantiated, used, and then discarded before resuming other Radiant
 * operations, as the implementation assumes that several basic mesh
 * characteristics will remain constant for the life of this object.
 *
 * @ingroup meshtex-core
 */
class MeshEntity
{
public: // public types

   /**
    * Values that represent the texture axes an operation should manipulate.
    */
   enum TextureAxisSelection
   {
      S_TEX_AXIS_ONLY,  ///< manipulate the S values
      T_TEX_AXIS_ONLY,  ///< manipulate the T values
      ALL_TEX_AXES      ///< manipulate both S and T values
   };

   /**
    * Values that represent the kinds of patch mesh slices. Note that the
    * assigned integer values are significant; do not change.
    */
   enum SliceType
   {
      ROW_SLICE_TYPE = 0,  ///< row 
      COL_SLICE_TYPE = 1,  ///< column 
      NUM_SLICE_TYPES = 2  ///< number of kinds of slice 
   };

   /**
    * Type for info/warning/error callbacks. The callback takes a const
    * char* argument (the message string); it has no return value.
    */
   typedef Callback1<const char *, void> MessageCallback;

   /**
    * Type for texture scale info callbacks. The callback takes two float
    * arguments (scale and tiles); it has no return value.
    */
   typedef Callback2<float, float, void> TexInfoCallback;

   /**
    * Type for defining how to manipulate control point or surface values
    * according to some linear combination of various values.
    */
   typedef struct {
      float oldValue;     ///< coefficient for original value
      float rowDistance;  ///< coefficient for surface distance along row
      float colDistance;  ///< coefficient for surface distance along col
      float rowNumber;    ///< coefficient for row number
      float colNumber;    ///< coefficient for column number
      float constant;     ///< constant
   } GeneralFunctionFactors;

   /**
    * Type for choosing a particular slice of a known kind (row or column).
    */
   typedef struct {
      bool maxSlice;  ///< if true, choose slice with highest index
      int index;      ///< if maxSlice is false, choose slice with this index
   } SliceDesignation;

   /**
    * Type for choosing a reference slice of a known kind (row or column) and
    * indicating how to use it for reference. Reference can be made to the total
    * slice surface length alone, or to each individual control point.
    */
   typedef struct {
      SliceDesignation designation;  ///< the slice
      bool totalLengthOnly;          ///< if true, reference total length only
   } RefSliceDescriptor;

   /**
    * An instance of this class can be used as a MeshEntity::TexInfoCallback, in
    * situations where the callback is a method to be invoked on a target
    * object. When invoking this constructor, the target object is the
    * constructor argument, and the target object class and method are template
    * parameters. The target object's method must have an appropriate signature
    * for TexInfoCallback: two float arguments, void return.
    */
   template<typename ObjectClass, void (ObjectClass::*member)(float, float)>
   class TexInfoCallbackMethod :
      public BindFirstOpaque2<Member2<ObjectClass, float, float, void, member> >
   {
   public:
      /**
       * Constructor.
       *
       * @param object The object on which to invoke the callback method.
       */
      TexInfoCallbackMethod(ObjectClass& object) :
         BindFirstOpaque2<Member2<ObjectClass, float, float, void, member> >(object) {}
   };

public: // public methods

   /// @name Lifecycle
   //@{
   MeshEntity(scene::Node& mesh,
              const MessageCallback& infoReportCallback,
              const MessageCallback& warningReportCallback,
              const MessageCallback& errorReportCallback);
   ~MeshEntity();
   //@}
   /// @name Interrogation
   //@{
   bool IsValid() const;
   void GetInfo(const int *refRow,
                const int *refCol,
                const TexInfoCallback *rowTexInfoCallback,
                const TexInfoCallback *colTexInfoCallback);
   //@}
   /// @name Simple modification
   //@{
   void MinAlign(TextureAxisSelection axes);
   void MaxAlign(TextureAxisSelection axes);
   void MinMaxAlignAutoScale(TextureAxisSelection axes);
   void MinMaxAlignStretch(TextureAxisSelection axes);
   void MinMaxAlignShrink(TextureAxisSelection axes);
   //@}
   /// @name Complex modification
   //@{
   void SetScale(SliceType sliceType,
                 const SliceDesignation *alignSlice,
                 const RefSliceDescriptor *refSlice,
                 bool naturalScale,
                 float naturalScaleOrTiles);
   void GeneralFunction(const GeneralFunctionFactors *sFactors,
                        const GeneralFunctionFactors *tFactors,
                        const SliceDesignation *alignRow,
                        const SliceDesignation *alignCol,
                        const RefSliceDescriptor *refRow,
                        const RefSliceDescriptor *refCol,
                        bool surfaceValues);
   //@}

private: // private methods

   /// @name Unimplemented to prevent copy/assignment
   //@{
   MeshEntity(const MeshEntity&);
   const MeshEntity& operator=(const MeshEntity&);
   //@}

private: // private types

   /**
    * Values that represent the kinds of texture axis.
    */
   enum TextureAxis
   {
      S_TEX_AXIS = 0,   ///< S texture axis 
      T_TEX_AXIS = 1,   ///< T texture axis
      NUM_TEX_AXES = 2  ///< number of kinds of texture axis
   };

   /**
    * Values that represent the kinds of position (spatial) axis.
    */
   enum PositionAxis
   {
      X_POS_AXIS = 0,   ///< X position axis 
      Y_POS_AXIS = 1,   ///< Y position axis
      Z_POS_AXIS = 2,   ///< Z position axis
      NUM_POS_AXES = 3  ///< number of kinds of position axis
   };

   /**
    * Values that represent ways of scaling a texture to make it aligned.
    */
   enum ScaleOperation
   {
      STRETCH_SCALE_OP,  ///< scale by stretching
      SHRINK_SCALE_OP    ///< scale by shrinking
   };

   /**
    * Type for orienting a slice within a particular patch.
    */
   typedef struct {
      SliceType sliceType; ///< slice type (row or column)
      float position;      ///< fractional dist from patch edge (0, 0.5, or 1)
      int edgeSlice[NUM_SLICE_TYPES];  ///< indices of slices at patch edges
   } SlicePatchContext;

   /**
    * Type for describing the application of a texture along a given slice,
    * on a specified texture axis.
    */
   typedef struct {
      float scale;  ///< texture scale along axis
      float tiles;  ///< # of times the texture tiles along axis
      float min;    ///< minimum value for that texture axis
      float max;    ///< maximum value for that texture axis
   } SliceTexInfo;

   /**
    * Type for internal representation of a reference slice of a given kind
    * (row or column), specifying the slice and indicating how to use it for
    * reference. Any external specification of "max slice" has been replaced
    * with an explicit slice number. Reference can be made to the total slice
    * length alone, or to the distance to each individual control point.
    */
   typedef struct {
      unsigned index;        ///< choose slice with this number
      bool totalLengthOnly;  ///< if true, reference total length only
   } RefSliceDescriptorInt;

   /**
    * Function signature for a private method that applies a preset
    * transformation on a given texture axis.
    */
   typedef bool(MeshEntity::*InternalImpl)(TextureAxis axis);

private: // private template methods

   /**
    * Utility template function for accessing a matrix element from code that
    * operates on either kind of slice.
    *
    * @param matrix    The matrix holding the mesh control points.
    * @param sliceType Slice kind (row or column).
    * @param slice     Slice number, among slices of that type in mesh.
    * @param index     Element index along the slice.
    *
    * @return The matrix element; can be used as lvalue or rvalue.
    */
   template<typename Element>
   inline static Element& MatrixElement(Matrix<Element>& matrix,
                                        SliceType sliceType,
                                        int slice,
                                        int index) {
      return (sliceType == ROW_SLICE_TYPE ? matrix(slice, index) : 
                                            matrix(index, slice));
   }

private: // private methods

   /// @name Internal state refresh
   //@{
   void UpdatePosMinMax(PositionAxis axis);
   void UpdateTexMinMax(TextureAxis axis);
   //@}
   /// @name Radiant state management
   //@{
   void CreateUndoPoint();
   void CommitChanges();
   //@}
   /// @name Argument resolution
   //@{
   int InternalSliceDesignation(const SliceDesignation *sliceDesignation,
                                SliceType sliceType);
   RefSliceDescriptorInt *InternalRefSliceDescriptor(const RefSliceDescriptor *refSlice,
                                                     SliceType sliceType,
                                                     RefSliceDescriptorInt& refSliceInt);
   //@}
   /// @name Subroutines for interrogation
   //@{
   float GetSliceTexScale(SliceType sliceType,
                          int slice,
                          TextureAxis axis,
                          float tiles);
   bool GetSliceTexInfo(SliceType sliceType,
                        int slice,
                        TextureAxis axis,
                        SliceTexInfo& info);
   void ReportSliceTexInfo(SliceType sliceType,
                           int slice,
                           TextureAxis axis,
                           char *messageBuffer,
                           unsigned messageBufferSize,
                           const TexInfoCallback *texInfoCallback);
   //@}
   /// @name Subroutines for simple modification
   //@{
   void ProcessForAxes(InternalImpl internalImpl,
                       TextureAxisSelection axes);
   void Shift(TextureAxis axis,
              float shift);
   void Scale(TextureAxis axis,
              float scale);
   bool MinAlignInt(TextureAxis axis);
   bool MaxAlignInt(TextureAxis axis);
   bool MinMaxAlignAutoScaleInt(TextureAxis axis);
   bool MinMaxAlignScale(TextureAxis axis,
                         ScaleOperation op);
   bool MinMaxAlignStretchInt(TextureAxis axis);
   bool MinMaxAlignShrinkInt(TextureAxis axis);
   //@}
   /// @name Surface measurement
   //@{
   float SliceParametricSpeedComponent(PositionAxis axis,
                                       float t,
                                       const SlicePatchContext& context);
   float SliceParametricSpeed(float t,
                              const SlicePatchContext& context);
   float EstimateSegmentLength(float startPosition,
                               float endPosition,
                               const SlicePatchContext& context);
   float RefineSegmentLength(float startPosition,
                             float endPosition,
                             const SlicePatchContext &context,
                             float segmentLengthEstimate,
                             float maxError);
   //@}
   /// @name Subroutines for complex modification
   //@{
   void GenControlTexFromSurface(TextureAxis axis,
                                 const Matrix<float>& surfaceValues);
   void CopyControlTexFromValues(TextureAxis axis,
                                 const Matrix<float>& values);
   void GenSurfaceFromControlTex(TextureAxis axis,
                                 Matrix<float>& surfaceValues);
   void CopyValuesFromControlTex(TextureAxis axis,
                                 Matrix<float>& values);
   void GenScaledDistanceValues(SliceType sliceType,
                                int alignSlice,
                                const RefSliceDescriptorInt *refSlice,
                                bool rawScale,
                                float rawScaleOrTiles,
                                Matrix<float>& values);
   void GeneralFunctionInt(const GeneralFunctionFactors& factors,
                           TextureAxis axis,
                           int alignRow,
                           int alignCol,
                           bool surfaceValues,
                           const Matrix<float>& rowDistances,
                           const Matrix<float>& colDistances);
   //@}

private: // private static member vars

   static TextureAxis _naturalAxis[NUM_SLICE_TYPES];
   static bool _radiantScaleInverted[NUM_SLICE_TYPES];
   static bool _radiantTilesInverted[NUM_SLICE_TYPES];
   static const char *_infoSliceFormatString[NUM_SLICE_TYPES];
   static const char *_infoSliceInfscaleFormatString[NUM_SLICE_TYPES];
   static const char *_warningSliceInfscaleFormatString[NUM_SLICE_TYPES];
   static const char *_errorBadSliceString[NUM_SLICE_TYPES];
   static const char *_errorSliceZeroscaleString[NUM_SLICE_TYPES];
   static const char *_errorSliceZerotilesString[NUM_SLICE_TYPES];

private: // private member vars

   /**
    * Handle for the Node object in Radiant that is the patch mesh entity.
    */
   scene::Node& _mesh;

   /**
    * Flag to indicate whether this object was properly generated from the
    * supplied entity.
    */
   bool _valid;

   /**
    * The control points of the mesh. Modifying the data in this matrix will
    * modify the mesh entity directly; it is NOT a copy of the entity's data.
    */
   PatchControlMatrix _meshData;

   /**
    * Callback function used to report information about the mesh.
    */
   const MessageCallback _infoReportCallback;

   /**
    * Callback function used to deliver warning messages.
    */
   const MessageCallback _warningReportCallback;

   /**
    * Callback function used to deliver error messages when operations on the
    * mesh fail.
    */
   const MessageCallback _errorReportCallback;

   /**
    * The number of grid units that would constitute a "natural" scale along
    * each texture axis, using the mesh's current texture. Radiant's natural
    * scale is 1/2 as many grid units as there are texture pixels.
    */
   float _naturalTexUnits[NUM_TEX_AXES];

   /**
    * The number of mesh slices of each kind (row or column).
    */
   unsigned _numSlices[NUM_SLICE_TYPES];

   /**
    * Whether the values for a texture axis have been modified since the last
    * time their min/max/aligned state was calculated.
    */
   bool _texMinMaxDirty[NUM_TEX_AXES];

   /**
    * The minimum values, across the entire mesh, for each texture axis.
    */
   float _texMin[NUM_TEX_AXES];

   /**
    * The maximum values, across the entire mesh, for each texture axis.
    */
   float _texMax[NUM_TEX_AXES];

   /**
    * Whether the minimum value for a texture axis is on a texture boundary.
    */
   bool _texMinAligned[NUM_TEX_AXES];

   /**
    * Whether the maximum value for a texture axis is on a texture boundary.
    */
   bool _texMaxAligned[NUM_TEX_AXES];

   /**
    * The minimum values, across the entire mesh, for each position axis.
    */
   float _posMin[NUM_POS_AXES];

   /**
    * The maximum values, across the entire mesh, for each position axis.
    */
   float _posMax[NUM_POS_AXES];
};

#endif // #if !defined(INCLUDED_MESHENTITY_H)