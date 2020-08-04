/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGTRANSFORM_H_
#define DOM_SVG_SVGTRANSFORM_H_

#include "gfxMatrix.h"
#include "mozilla/dom/SVGTransformBinding.h"
#include "mozilla/gfx/Matrix.h"
#include "nsDebug.h"

namespace mozilla {

/*
 * The DOM wrapper class for this class is DOMSVGTransform.
 */
class SVGTransform {
 public:
  // Default ctor initialises to matrix type with identity matrix
  SVGTransform()
      : mMatrix()  // Initialises to identity
        ,
        mAngle(0.f),
        mOriginX(0.f),
        mOriginY(0.f),
        mType(dom::SVGTransform_Binding::SVG_TRANSFORM_MATRIX) {}

  explicit SVGTransform(const gfxMatrix& aMatrix)
      : mMatrix(aMatrix),
        mAngle(0.f),
        mOriginX(0.f),
        mOriginY(0.f),
        mType(dom::SVGTransform_Binding::SVG_TRANSFORM_MATRIX) {}

  bool operator==(const SVGTransform& rhs) const {
    return mType == rhs.mType && MatricesEqual(mMatrix, rhs.mMatrix) &&
           mAngle == rhs.mAngle && mOriginX == rhs.mOriginX &&
           mOriginY == rhs.mOriginY;
  }

  void GetValueAsString(nsAString& aValue) const;

  float Angle() const { return mAngle; }
  void GetRotationOrigin(float& aOriginX, float& aOriginY) const {
    aOriginX = mOriginX;
    aOriginY = mOriginY;
  }
  uint16_t Type() const { return mType; }

  const gfxMatrix& GetMatrix() const { return mMatrix; }
  void SetMatrix(const gfxMatrix& aMatrix);
  void SetTranslate(float aTx, float aTy);
  void SetScale(float aSx, float aSy);
  void SetRotate(float aAngle, float aCx, float aCy);
  nsresult SetSkewX(float aAngle);
  nsresult SetSkewY(float aAngle);

  static bool MatricesEqual(const gfxMatrix& a, const gfxMatrix& b) {
    return a._11 == b._11 && a._12 == b._12 && a._21 == b._21 &&
           a._22 == b._22 && a._31 == b._31 && a._32 == b._32;
  }

 protected:
  gfxMatrix mMatrix;
  float mAngle, mOriginX, mOriginY;
  uint16_t mType;
};

/*
 * A slightly more light-weight version of SVGTransform for SMIL animation.
 *
 * Storing the parameters in an array (rather than a matrix) also allows simpler
 * (transform type-agnostic) interpolation and addition.
 *
 * The meaning of the mParams array depends on the transform type as follows:
 *
 * Type                | mParams[0], mParams[1], mParams[2], ...
 * --------------------+-----------------------------------------
 * translate           | tx, ty
 * scale               | sx, sy
 * rotate              | rotation-angle (in degrees), cx, cy
 * skewX               | skew-angle (in degrees)
 * skewY               | skew-angle (in degrees)
 * matrix              | a, b, c, d, e, f
 *
 * The matrix type is never generated by animation code (it is only produced
 * when the user inserts one via the DOM) and often requires special handling
 * when we do encounter it. Therefore many users of this class are only
 * interested in the first three parameters and so we provide a special
 * constructor for setting those parameters only.
 */
class SVGTransformSMILData {
 public:
  // Number of float-params required in constructor, if constructing one of the
  // 'simple' transform types (all but matrix type)
  static const uint32_t NUM_SIMPLE_PARAMS = 3;

  // Number of float-params required in constructor for matrix type.
  // This is also the number of params we actually store, regardless of type.
  static const uint32_t NUM_STORED_PARAMS = 6;

  explicit SVGTransformSMILData(uint16_t aType) : mTransformType(aType) {
    MOZ_ASSERT(aType >= dom::SVGTransform_Binding::SVG_TRANSFORM_MATRIX &&
                   aType <= dom::SVGTransform_Binding::SVG_TRANSFORM_SKEWY,
               "Unexpected transform type");
    for (uint32_t i = 0; i < NUM_STORED_PARAMS; ++i) {
      mParams[i] = 0.f;
    }
  }

  SVGTransformSMILData(uint16_t aType, float (&aParams)[NUM_SIMPLE_PARAMS])
      : mTransformType(aType) {
    MOZ_ASSERT(aType >= dom::SVGTransform_Binding::SVG_TRANSFORM_TRANSLATE &&
                   aType <= dom::SVGTransform_Binding::SVG_TRANSFORM_SKEWY,
               "Expected 'simple' transform type");
    for (uint32_t i = 0; i < NUM_SIMPLE_PARAMS; ++i) {
      mParams[i] = aParams[i];
    }
    for (uint32_t i = NUM_SIMPLE_PARAMS; i < NUM_STORED_PARAMS; ++i) {
      mParams[i] = 0.f;
    }
  }

  // Conversion to/from a fully-fledged SVGTransform
  explicit SVGTransformSMILData(const SVGTransform& aTransform);
  SVGTransform ToSVGTransform() const;

  bool operator==(const SVGTransformSMILData& aOther) const {
    if (mTransformType != aOther.mTransformType) return false;

    for (uint32_t i = 0; i < NUM_STORED_PARAMS; ++i) {
      if (mParams[i] != aOther.mParams[i]) {
        return false;
      }
    }

    return true;
  }

  bool operator!=(const SVGTransformSMILData& aOther) const {
    return !(*this == aOther);
  }

  uint16_t mTransformType;
  float mParams[NUM_STORED_PARAMS];
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGTRANSFORM_H_
