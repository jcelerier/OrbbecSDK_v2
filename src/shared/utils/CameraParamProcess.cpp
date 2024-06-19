#include "CameraParamProcess.hpp"

namespace libobsensor {
void CameraParamProcessor::cameraIntrinsicParamsMirror(OBCameraIntrinsic *intrinsic) {
    intrinsic->cx = (float)1.0 * intrinsic->width - intrinsic->cx - 1;
}

void CameraParamProcessor::distortionParamMirror(OBCameraDistortion *distort) {
    distort->p2 *= -1.0;
}

void CameraParamProcessor::d2cTransformParamsMirror(OBD2CTransform *transform) {
    // 旋转矩阵中r1、r2、r3和r6求反
    (transform->rot)[1] *= -1.0;
    (transform->rot)[2] *= -1.0;
    (transform->rot)[3] *= -1.0;
    (transform->rot)[6] *= -1.0;
    // 平移向量中tx求反
    (transform->trans)[0] *= -1.0;
}

void CameraParamProcessor::cameraIntrinsicParamsFlip(OBCameraIntrinsic *intrinsic) {
    intrinsic->cy = (float)1.0 * intrinsic->height - intrinsic->cy - 1;
}

void CameraParamProcessor::distortionParamFlip(OBCameraDistortion *distort) {
    distort->p1 *= -1.0;
}

void CameraParamProcessor::d2cTransformParamsFlip(OBD2CTransform *transform) {
    (transform->rot)[1] *= -1.0;
    (transform->rot)[3] *= -1.0;
    (transform->rot)[5] *= -1.0;
    (transform->rot)[7] *= -1.0;
    (transform->trans)[1] *= -1.0;
}

void CameraParamProcessor::mirrorCameraParam(OBCameraParam *cameraParam) {
    CameraParamProcessor::cameraIntrinsicParamsMirror(&cameraParam->rgbIntrinsic);
    CameraParamProcessor::cameraIntrinsicParamsMirror(&cameraParam->depthIntrinsic);

    CameraParamProcessor::distortionParamMirror(&cameraParam->rgbDistortion);
    CameraParamProcessor::distortionParamMirror(&cameraParam->depthDistortion);

    CameraParamProcessor::d2cTransformParamsMirror(&cameraParam->transform);
}

void CameraParamProcessor::flipCameraParam(OBCameraParam *cameraParam) {
    CameraParamProcessor::cameraIntrinsicParamsFlip(&cameraParam->rgbIntrinsic);
    CameraParamProcessor::cameraIntrinsicParamsFlip(&cameraParam->depthIntrinsic);

    CameraParamProcessor::distortionParamFlip(&cameraParam->rgbDistortion);
    CameraParamProcessor::distortionParamFlip(&cameraParam->depthDistortion);

    CameraParamProcessor::d2cTransformParamsFlip(&cameraParam->transform);
}

void CameraParamProcessor::cameraIntrinsicParamsRotate90(OBCameraIntrinsic *intrinsic) {
    auto tmp      = intrinsic->fx;
    intrinsic->fx = intrinsic->fy;
    intrinsic->fy = tmp;
    tmp           = intrinsic->cx;
    intrinsic->cx = (float)1.0 * intrinsic->height - intrinsic->cy - 1;
    intrinsic->cy = tmp;

    int16_t resTmp    = intrinsic->width;
    intrinsic->width  = intrinsic->height;
    intrinsic->height = resTmp;
}

void CameraParamProcessor::distortionParamRotate90(OBCameraDistortion *distort) {
    float tmp   = distort->p1;
    distort->p1 = distort->p2;
    distort->p2 = (float)-1.0 * tmp;
}

void CameraParamProcessor::d2cTransformParamsRotate90(OBD2CTransform *transform) {
    (transform->rot)[1] *= 1.0;
    (transform->rot)[3] *= 1.0;
    (transform->rot)[5] *= 1.0;
    (transform->rot)[7] *= 1.0;
    float tmp             = (transform->trans)[0];
    (transform->trans)[0] = (float)-1.0 * (transform->trans)[1];
    (transform->trans)[1] = tmp;
}

void CameraParamProcessor::cameraIntrinsicParamsRotate180(OBCameraIntrinsic *intrinsic) {
    intrinsic->cx = (float)1.0 * intrinsic->width - intrinsic->cx - 1;
    intrinsic->cy = (float)1.0 * intrinsic->height - intrinsic->cy - 1;
}

void CameraParamProcessor::distortionParamRotate180(OBCameraDistortion *distort) {
    distort->p1 *= -1.0;
    distort->p2 *= -1.0;
}

void CameraParamProcessor::d2cTransformParamsRotate180(OBD2CTransform *transform) {
    (transform->rot)[2] *= -1.0;
    (transform->rot)[5] *= -1.0;
    (transform->rot)[6] *= -1.0;
    (transform->rot)[7] *= -1.0;
    (transform->trans)[0] *= -1.0;
    (transform->trans)[1] *= -1.0;
}

void CameraParamProcessor::cameraIntrinsicParamsRotate270(OBCameraIntrinsic *intrinsic) {
    auto tmp      = intrinsic->fx;
    intrinsic->fx = intrinsic->fy;
    intrinsic->fy = tmp;

    tmp           = intrinsic->cy;
    intrinsic->cy = (float)1.0 * intrinsic->width - intrinsic->cx - 1;
    intrinsic->cx = tmp;

    int16_t resTmp    = intrinsic->width;
    intrinsic->width  = intrinsic->height;
    intrinsic->height = resTmp;
}

void CameraParamProcessor::distortionParamRotate270(OBCameraDistortion *distort) {
    float tmp   = distort->p1;
    distort->p1 = (float)-1.0 * distort->p2;
    distort->p2 = tmp;
}

void CameraParamProcessor::d2cTransformParamsRotate270(OBD2CTransform *transform) {
    float tmp;
    (transform->rot)[2] *= 1.0;
    (transform->rot)[5] *= 1.0;
    (transform->rot)[6] *= 1.0;
    (transform->rot)[7] *= 1.0;
    tmp                   = (transform->trans)[0];
    (transform->trans)[0] = (transform->trans)[1];
    (transform->trans)[1] = (float)-1.0 * tmp;
}

void CameraParamProcessor::rotateCameraParam(OBCameraParam *cameraParam, int rotateAngle) {
    // TODO:数值需要改为枚举
    switch(rotateAngle) {
    case 90: {
        CameraParamProcessor::cameraIntrinsicParamsRotate90(&cameraParam->rgbIntrinsic);
        CameraParamProcessor::cameraIntrinsicParamsRotate90(&cameraParam->depthIntrinsic);
        CameraParamProcessor::distortionParamRotate90(&cameraParam->rgbDistortion);
        CameraParamProcessor::distortionParamRotate90(&cameraParam->depthDistortion);
        CameraParamProcessor::d2cTransformParamsRotate90(&cameraParam->transform);
    } break;

    case 180: {
        CameraParamProcessor::cameraIntrinsicParamsRotate180(&cameraParam->rgbIntrinsic);
        CameraParamProcessor::cameraIntrinsicParamsRotate180(&cameraParam->depthIntrinsic);
        CameraParamProcessor::distortionParamRotate180(&cameraParam->rgbDistortion);
        CameraParamProcessor::distortionParamRotate180(&cameraParam->depthDistortion);
        CameraParamProcessor::d2cTransformParamsRotate180(&cameraParam->transform);
    } break;

    case 270: {
        CameraParamProcessor::cameraIntrinsicParamsRotate270(&cameraParam->rgbIntrinsic);
        CameraParamProcessor::cameraIntrinsicParamsRotate270(&cameraParam->depthIntrinsic);
        CameraParamProcessor::distortionParamRotate270(&cameraParam->rgbDistortion);
        CameraParamProcessor::distortionParamRotate270(&cameraParam->depthDistortion);
        CameraParamProcessor::d2cTransformParamsRotate270(&cameraParam->transform);
    } break;
    }
}

OBExtrinsic CameraParamProcessor::multiplyExtrinsic(const OBExtrinsic &extrinsic1, const OBExtrinsic &extrinsic2) {
    OBExtrinsic resultExtrinsic;

    // Multiply rotation matrices
    for(int i = 0; i < 3; i++) {
        for(int j = 0; j < 3; j++) {
            resultExtrinsic.rot[i * 3 + j] = extrinsic1.rot[i * 3] * extrinsic2.rot[j] + extrinsic1.rot[i * 3 + 1] * extrinsic2.rot[j + 3]
                                             + extrinsic1.rot[i * 3 + 2] * extrinsic2.rot[j + 6];
        }
    }

    // Calculate the translation vector
    resultExtrinsic.trans[0] = extrinsic1.rot[0] * extrinsic2.trans[0] +  //
                               extrinsic1.rot[1] * extrinsic2.trans[1] +  //
                               extrinsic1.rot[2] * extrinsic2.trans[2] +  //
                               extrinsic1.trans[0];

    resultExtrinsic.trans[1] = extrinsic1.rot[3] * extrinsic2.trans[0] +  //
                               extrinsic1.rot[4] * extrinsic2.trans[1] +  //
                               extrinsic1.rot[5] * extrinsic2.trans[2] +  //
                               extrinsic1.trans[1];

    resultExtrinsic.trans[2] = extrinsic1.rot[6] * extrinsic2.trans[0] +  //
                               extrinsic1.rot[7] * extrinsic2.trans[1] +  //
                               extrinsic1.rot[8] * extrinsic2.trans[2] +  //
                               extrinsic1.trans[2];

    return resultExtrinsic;
}

OBExtrinsic CameraParamProcessor::inverseExtrinsic(const OBExtrinsic &extrinsic) {
    OBExtrinsic invExtrinsic;

    // Transpose the rotation matrix
    invExtrinsic.rot[0] = extrinsic.rot[0];
    invExtrinsic.rot[1] = extrinsic.rot[3];
    invExtrinsic.rot[2] = extrinsic.rot[6];
    invExtrinsic.rot[3] = extrinsic.rot[1];
    invExtrinsic.rot[4] = extrinsic.rot[4];
    invExtrinsic.rot[5] = extrinsic.rot[7];
    invExtrinsic.rot[6] = extrinsic.rot[2];
    invExtrinsic.rot[7] = extrinsic.rot[5];
    invExtrinsic.rot[8] = extrinsic.rot[8];

    // Calculate the inverse translation vector
    float invRot[9] = { invExtrinsic.rot[0], invExtrinsic.rot[1], invExtrinsic.rot[2], invExtrinsic.rot[3], invExtrinsic.rot[4],
                        invExtrinsic.rot[5], invExtrinsic.rot[6], invExtrinsic.rot[7], invExtrinsic.rot[8] };

    invExtrinsic.trans[0] = -invRot[0] * extrinsic.trans[0] - invRot[1] * extrinsic.trans[1] - invRot[2] * extrinsic.trans[2];
    invExtrinsic.trans[1] = -invRot[3] * extrinsic.trans[0] - invRot[4] * extrinsic.trans[1] - invRot[5] * extrinsic.trans[2];
    invExtrinsic.trans[2] = -invRot[6] * extrinsic.trans[0] - invRot[7] * extrinsic.trans[1] - invRot[8] * extrinsic.trans[2];

    return invExtrinsic;
}

}  // namespace libobsensor