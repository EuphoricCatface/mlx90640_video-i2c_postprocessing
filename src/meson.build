mlx90640_video_i2c_postprocessing_sources = [
    'main.cpp',
    'mlx90640.cpp',
    'dev_handler.cpp',
    'push_data.cpp'
]

mlx90640_video_i2c_postprocessing_deps = [
    dependency('gstreamer-1.0'),
    dependency('gstreamer-video-1.0'),
    dependency('gstreamer-app-1.0'),
    dependency('gstreamer-controller-1.0'),
]

executable('mlx90640_video-i2c_postprocessing', mlx90640_video_i2c_postprocessing_sources,
    dependencies: mlx90640_video_i2c_postprocessing_deps,
    include_directories : include_directories('../include'),
    install: true,
)
