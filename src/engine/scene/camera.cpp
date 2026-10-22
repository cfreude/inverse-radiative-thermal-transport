#include <tamashii/engine/scene/camera.hpp>
#include <tamashii/engine/render/render_system.hpp>
#include <tamashii/engine/platform/system.hpp>
#include <tamashii/engine/common/common.hpp>

#include <cmath>

T_USE_NAMESPACE

Camera::Camera(const std::string& aName) : Asset(Asset::Type::CAMERA, aName) {}

Camera* Camera::alloc(const std::string& aName)
{
	return new Camera(aName); 
}

void Camera::initPerspectiveCamera(const float aYFov, const float aAspectRatio, const float aZNear, const float aZFar)
{
	this->mType = Type::PERSPECTIVE;
	this->mAspectRatio = aAspectRatio;
	this->mYFov = aYFov;
	this->mZFar = aZFar;
	this->mZNear = aZNear;
	calculateProjectionMatrix();
}

void Camera::initOrthographicCamera(const float aXMag, const float aYMag, const float aZNear, const float aZFar)
{
	mType = Type::ORTHOGRAPHIC;
	mXMag = aXMag;
	mYMag = aYMag;
	mZFar = aZFar;
	mZNear = aZNear;
	calculateProjectionMatrix();
}

void Camera::updateAspectRatio(const float aAspectRatio)
{
	mAspectRatio = aAspectRatio;
}

void Camera::updateMag(const float aXMag, const float aYMag)
{
	mXMag = aXMag;
	mYMag = aYMag;
}

bool Camera::updateAspectRatio(const glm::uvec2 aSize)
{
	if(glm::any(glm::notEqual(mSize, aSize)))
	{
		mSize = aSize;
		mAspectRatio = static_cast<float>(aSize.x) / static_cast<float>(aSize.y);
		return true;
	}
	return false;
}

bool Camera::updateMag(const glm::uvec2 aSize)
{
	if (glm::any(glm::notEqual(mSize, aSize)))
	{
		mSize = aSize;
		mAspectRatio = (static_cast<float>(aSize.x) * 0.5f) / (static_cast<float>(aSize.y) * 0.5f);
		return true;
	}
	return false;
}

glm::mat4 Camera::getProjectionMatrix()
{
	if (mType == Type::PERSPECTIVE) {
        mProjectionMatrix[0][0] = 1.0f / (mAspectRatio * std::tan(0.5f * mYFov));
	}
	return mProjectionMatrix;
}

Camera::Type Camera::getType() const
{
	return mType;
}

float Camera::getZFar() const
{
	return mZFar;
}

float Camera::getZNear() const
{
	return mZNear;
}

float Camera::getAspectRation() const
{
	return mAspectRatio;
}

float Camera::getYFov() const
{
	return mYFov;
}

float Camera::getXMag() const
{
	return mXMag;
}

float Camera::getYMag() const
{
	return mYMag;
}

void Camera::calculateProjectionMatrix()
{
	if (mType == Type::PERSPECTIVE) {
		const bool infinite = (mZFar == 0.0f);
		if (!infinite) this->mProjectionMatrix = glm::perspective(mYFov, mAspectRatio, mZNear, mZFar);
		else mProjectionMatrix = glm::infinitePerspective(mYFov, mAspectRatio, mZNear);
	}
	else if (mType == Type::ORTHOGRAPHIC) {
		const float zero = 0;
		mProjectionMatrix = glm::ortho(zero, mXMag, zero, mYMag, mZNear, mZFar);
	}
}

glm::mat4 Camera::flipY(glm::mat4 aViewMatrix)
{
	const glm::mat4 flip_matrix = {
	 1,   0,  0,  0,
	 0,  -1,  0,  0,
	 0,   0,  1,  0,
	 0,   0,  0,  1
	};
	return aViewMatrix = flip_matrix * aViewMatrix;
}
// formula https://gsteph.blogspot.com/2012/05/world-view-and-projection-matrix.html
// this is the inverse camera matrix, also called view matrix
void Camera::buildViewMatrix(float* aViewMatrix, glm::vec3 const& aPosition, glm::vec3 const& aRight, glm::vec3 const& aUp, glm::vec3 const& aForward) {
	// first column
	aViewMatrix[0] = aRight[0];
	aViewMatrix[4] = aRight[1];
	aViewMatrix[8] = aRight[2];
	aViewMatrix[12] = (-aPosition[0] * aRight[0] + -aPosition[1] * aRight[1] + -aPosition[2] * aRight[2]);
	// second column
	aViewMatrix[1] = aUp[0];
	aViewMatrix[5] = aUp[1];
	aViewMatrix[9] = aUp[2];
	aViewMatrix[13] = (-aPosition[0] * aUp[0] + -aPosition[1] * aUp[1] + -aPosition[2] * aUp[2]);
	// third column
	aViewMatrix[2] = aForward[0];
	aViewMatrix[6] = aForward[1];
	aViewMatrix[10] = aForward[2];
	aViewMatrix[14] = (-aPosition[0] * aForward[0] + -aPosition[1] * aForward[1] + -aPosition[2] * aForward[2]);
	// fourth column
	aViewMatrix[3] = 0;
	aViewMatrix[7] = 0;
	aViewMatrix[11] = 0;
	aViewMatrix[15] = 1;
}

void Camera::buildModelMatrix(float* aModelMatrix, glm::vec3 const& aPosition, glm::vec3 const& aRight,
	glm::vec3 const& aUp, glm::vec3 const& aForward)
{
	aModelMatrix[0] = aRight[0]; aModelMatrix[1] = aRight[1]; aModelMatrix[2] = aRight[2]; aModelMatrix[3] = 0;
	aModelMatrix[4] = aUp[0]; aModelMatrix[5] = aUp[1]; aModelMatrix[6] = aUp[2]; aModelMatrix[7] = 0;
	aModelMatrix[8] = aForward[0]; aModelMatrix[9] = aForward[1]; aModelMatrix[10] = aForward[2]; aModelMatrix[11] = 0;
	aModelMatrix[12] = aPosition[0]; aModelMatrix[13] = aPosition[1]; aModelMatrix[14] = aPosition[2]; aModelMatrix[15] = 1;
}
