#include "FollowComponent.h"
#include "Transform.h"
#include "EngineTime.h"
#include "Entity.h"
#include "Input.h"

#include <iostream>

FollowComponent::FollowComponent()
{
}

FollowComponent::~FollowComponent()
{
}

void FollowComponent::setTarget(Transform* target)
{
	mTarget = target;
}

void FollowComponent::setOffset(float offset)
{
	mOffset = offset;
}

void FollowComponent::setSpeed(float speed)
{
	mSpeed = speed;
}

void FollowComponent::setMouseSpeed(float mouseSpeed)
{
	mMouseSpeed = mouseSpeed;
}

void FollowComponent::update()
{
    if (!mTarget) return;

	if (Input::getMouseWheelUp()) {
		mOffset -= 0.1f;
	}
	if (Input::getMouseWheelDown()) {
		mOffset += 0.1f;
	}

    Vector2 md = Input::getDeltaMousePos();
    mYaw = md.x * mMouseSpeed;
    mPitch = md.y * mMouseSpeed;

    Vector3 baseDir = (mTarget->getPosition() - mOwner->getTransform()->getPosition()).normalized();
    baseDir = mOwner->getTransform()->getForward();

    Vector3 up = mTarget->getUp().normalized();
    Vector3 yawedDir = Vector3::rotateAroundUp(baseDir, up, mYaw);

    Vector3 right = up.cross(yawedDir).normalized();

    Vector3 finalDir = Vector3::rotateAroundUp(yawedDir, right, mPitch).normalized();

    Vector3 desiredPos = mTarget->getPosition() - mOwner->getTransform()->getForward() * mOffset;

    Vector3 curPos = mOwner->getTransform()->getPosition();
    Vector3 delta = desiredPos - curPos;
    Vector3 newPos = curPos + delta * (mSpeed * Time::getDeltaTime());

    mOwner->getTransform()->setPosition(desiredPos);
    mOwner->getTransform()->setForward(finalDir);
}
