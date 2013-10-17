/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. As clarification, there
 * is no requirement that the copyright notice and permission be included in
 * binary distributions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef CAMERA_H
#define CAMERA_H

#include <iostream>
#include <d3d10.h>
#include <d3dx10.h>
#include <dxerr.h>
#include <dxgi.h>

class Camera {
    public:
        Camera() :
            distance(0.0f),
            angle(0.0f, 0.0f),
            angularVelocity(0.0f, 0.0f),
            panPosition(0.0f, 0.0f),
            viewportSize(1.0f, 1.0f),
            mousePos(0.0f, 0.0f),
            attenuation(0.0f),
            draggingLeft(false),
            draggingMiddle(false),
            draggingRight(false) { build(); }

        LRESULT handleMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

        void frameMove(FLOAT elapsedTime);

        void setDistance(float distance) { this->distance = distance; }
        float getDistance() const { return distance; }

        void setPanPosition(const D3DXVECTOR2 &panPosition) { this->panPosition = panPosition; }
        const D3DXVECTOR2 &getPanPosition() const { return panPosition; }

        void setAngle(const D3DXVECTOR2 &angle) { this->angle = angle; }
        const D3DXVECTOR2 &getAngle() const { return angle; }

        void setAngularVelocity(const D3DXVECTOR2 &angularVelocity) { this->angularVelocity = angularVelocity; }
        const D3DXVECTOR2 &getAngularVelocity() const { return angularVelocity; }

        void setProjection(float fov, float aspect, float zn, float zf);
        void setJitteredProjection(float fov, float aspect, float zn, float zf, const D3DXVECTOR2 &jitter);
        void setViewportSize(const D3DXVECTOR2 &viewportSize) { this->viewportSize = viewportSize; }

        const D3DXMATRIX &getViewMatrix() { return view; }
        const D3DXMATRIX &getProjectionMatrix() const { return projection; }

        const D3DXVECTOR3 &getLookAtPosition() { return lookAtPosition; }  
        const D3DXVECTOR3 &getEyePosition() { return eyePosition; }  

        friend std::ostream& operator <<(std::ostream &os, const Camera &camera);
        friend std::istream& operator >>(std::istream &is, Camera &camera);

    private:
        void build();
        void updatePosition(D3DXVECTOR2 delta);

        D3DXMATRIX jitteredFrustum(float left, float right, float bottom, float top, float zn, float zf, 
                                   float pixdx, float pixdy, float eyedx, float eyedy, float focus) const;
        D3DXMATRIX jitteredPerspective(float fovy, float aspect, float zn, float zf, 
                                       float pixdx, float pixdy, float eyedx, float eyedy, float focus) const;

        float distance;
        D3DXVECTOR2 panPosition;
        D3DXVECTOR2 angle, angularVelocity;
        D3DXVECTOR2 viewportSize;

        D3DXMATRIX view, projection;
        D3DXVECTOR3 lookAtPosition;
        D3DXVECTOR3 eyePosition;

        D3DXVECTOR2 mousePos;
        float attenuation;
        bool draggingLeft, draggingMiddle, draggingRight;
};

#endif
