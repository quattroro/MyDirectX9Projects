/**
 * a render target 
 */
class FRenderTarget
{
    FTextureRHIRef RenderTargetTextureRHI;
};

/**
 * encapsulate the I/O of a viewport
 * the viewport display is implemented using the platform independent RHI 
 */
class FViewport : public FRenderTarget, protected FRenderResource
{

};