# import replicate

# output = replicate.run(
#     "zsxkib/instant-id:491ddf5be6b827f8931f088ef10c6d015f6d99685e6454e6f04c8ac298979686",
#     input={
#         "image": open("/home/nino/Sherlocked_Face_Inator/FACES/face_1.jpg", "rb"),
#         "prompt": "a sketch of a man dressed in alchemist clothes wearing glasses, unrefined",
#         "scheduler": "DPMSolverMultistepScheduler-Karras-SDE",
#         "enable_lcm": False,
#         "num_outputs": 1,
#         "sdxl_weights": "pony-diffusion-v6-xl",
#         "output_format": "png",
#         "pose_strength": 0,
#         "canny_strength": 0.3,
#         "depth_strength": 0.5,
#         "guidance_scale": 2.5,
#         "output_quality": 100,
#         "ip_adapter_scale": 0.8,
#         "lcm_guidance_scale": 1.5,
#         "num_inference_steps": 30,
#         "enable_pose_controlnet": True,
#         "enhance_nonface_region": True,
#         "enable_canny_controlnet": False,
#         "enable_depth_controlnet": False,
#         "lcm_num_inference_steps": 5,
#         "local_files_only": False,
#         "controlnet_conditioning_scale": 0.8
#     }
# )

import replicate

input = {
    "image": open("/home/nino/Sherlocked_Face_Inator/FACES/face_1.jpg", "rb"),
    "prompt": "a quick, rough sketch of a man dressed in alchemist clothes wearing glasses, unrefined, with pencil strokes",
    "sdxl_weights": "dreamshaper-xl",
    "guidance_scale": 2.5,
    "ip_adapter_scale": 0.8,
    "num_inference_steps": 15,
    "controlnet_conditioning_scale": 0.8,
    "output_format": "png",
    "num_outputs": 2,
    "negative_prompt": "text, nsfw, realistic"
}

output = replicate.run(
    "zsxkib/instant-id:491ddf5be6b827f8931f088ef10c6d015f6d99685e6454e6f04c8ac298979686",
    input=input
)
print(output)
#=> ["https://replicate.delivery/pbxt/OundoMAAs07sEJ8OLTdCulA...



