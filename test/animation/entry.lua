local runtime = import_package "ant.imguibase".runtime
runtime.start {
	policy = {
		"ant.animation|animation",
		"ant.animation|animation_controller.state_machine",
		"ant.animation|ozzmesh",
		"ant.animation|ozz_skinning",
		"ant.animation|skinning",
		"ant.serialize|serialize",
		"ant.render|mesh",
		"ant.render|render",
		"ant.render|name",
		"ant.render|light.directional",
		"ant.render|light.ambient",
	},
	system = {
		"ant.test.animation|init_loader",
	},
	pipeline = {
		{ name = "init",
			"init",
			"init_blit_render",
			"post_init",
		},
		{ name = "exit",
			"exit",
		},
		{ name = "update",
			"timer",
			"data_changed",
			{name = "collider",
				"update_collider_transform",
				"update_collider",
			},
			{ name = "animation",
				"animation_state",
				"sample_animation_pose",
				"skin_mesh",
			},
			{ name = "sky",
				"update_sun",
				"update_sky",
			},
			"widget",
			{ name = "render",
				"shadow_camera",
				"load_render_properties",
				"filter_primitive",
				"make_shadow",
				"debug_shadow",
				"cull",
				"render_commit",
				{ name = "postprocess",
					"bloom",
					"tonemapping",
					"combine_postprocess",
				}
			},
			"camera_control",
			"lock_target",
			"pickup",
			{ name = "ui",
				"ui_start",
				"ui_update",
				"ui_end",
			},
			"end_frame",
			"final",
		}
	}
}
