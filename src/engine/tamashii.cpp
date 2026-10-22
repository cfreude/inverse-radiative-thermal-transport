#include <tamashii/tamashii.hpp>
#include <tamashii/engine/common/common.hpp>
#include <tamashii/engine/render/render_backend_implementation.hpp>

void tamashii::init(int argc, char* argv[]) {
	if (argc > 1) {
		Common::getInstance().init(argc - 1, &argv[1], nullptr);
	}
	else {
		Common::getInstance().init(0, nullptr, nullptr);
	}
}

void tamashii::shutdown() {
	Common::getInstance().shutdown();
}

void tamashii::whileFrame() {
	while (Common::getInstance().frame());
}

void tamashii::run(int argc, char* argv[]) {
	init(argc, argv);
	// main loop for frame
	while (Common::getInstance().frame());
}

void tamashii::addBackend(RenderBackend* aRenderBackend){
    tamashii::Common::getInstance().getRenderSystem()->addBackend(aRenderBackend);
}

tamashii::RenderBackendImplementation* tamashii::getCurrentRenderBackendImplementation()
{
	return tamashii::Common::getInstance().getRenderSystem()->getCurrentBackendImplementations();
}

std::vector<tamashii::RenderBackendImplementation*>& tamashii::getAvailableRenderBackendImplementations()
{
	return tamashii::Common::getInstance().getRenderSystem()->getAvailableBackendImplementations();
}

tamashii::RenderBackendImplementation* tamashii::findBackendImplementation(const std::string& aName)
{
	const std::vector<tamashii::RenderBackendImplementation*>& impls = tamashii::Common::getInstance().getRenderSystem()->getAvailableBackendImplementations();
	for(tamashii::RenderBackendImplementation* impl : impls)
	{
		if (impl->getName() == aName) return impl;
	}
	return nullptr;
}
