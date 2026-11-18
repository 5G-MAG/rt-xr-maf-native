/*
 * Copyright (c) 2023 MotionSpell
 * Licensed under the License terms and conditions for use, reproduction,
 * and distribution of 5GMAG software (the “License”).
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at https://www.5g-mag.com/license .
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an “AS IS” BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */

#include "Backend.hpp"

#import <Metal/Metal.h>

#include "IUnityGraphics.h"
#include "IUnityGraphicsMetal.h"
#include "Importers.hpp"

namespace
{

    class MetalBackend : public GraphicsBackend
    {
    private:
        IUnityGraphicsMetalV1 *metal_;
        std::unique_ptr<PictureImporter> importer_;

    public:
        MetalBackend(IUnityGraphicsMetalV1 *metal, std::unique_ptr<PictureImporter> importer) : metal_(metal), importer_(std::move(importer)) {}

        PictureImporter &Importer() override
        {
            return *importer_;
        }

        void Release(TextureRegistry &registry) override
        {
            const uint64_t batch = registry.RetireBatch();
            if (batch == 0)
            {
                return;
            }
            // the frames retired before the event are no longer used once the commands recorded until now complete
            id<MTLCommandBuffer> commands = metal_->CurrentCommandBuffer();
            if (commands)
            {
                TextureRegistry *r = &registry;
                [commands addCompletedHandler:^(id<MTLCommandBuffer>) {
                  r->Complete(batch);
                }];
            }
            else
            {
                registry.Complete(batch);
            }
        }
    };

} // namespace

std::unique_ptr<GraphicsBackend> CreateMetalBackend(IUnityInterfaces *interfaces)
{
    IUnityGraphics *graphics = interfaces->Get<IUnityGraphics>();
    if (!graphics || graphics->GetRenderer() != kUnityGfxRendererMetal)
    {
        return nullptr;
    }
    IUnityGraphicsMetalV1 *metal = interfaces->Get<IUnityGraphicsMetalV1>();
    if (!metal)
    {
        return nullptr;
    }
    std::unique_ptr<PictureImporter> importer = CreateMetalImporter(metal->MetalDevice());
    if (!importer)
    {
        return nullptr;
    }
    return std::make_unique<MetalBackend>(metal, std::move(importer));
}
