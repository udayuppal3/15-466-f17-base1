#include "load_save_png.hpp"
#include "GL.hpp"

#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <iostream>
#include <stdexcept>

static GLuint compile_shader(GLenum type, std::string const &source);
static GLuint link_program(GLuint vertex_shader, GLuint fragment_shader);

int main(int argc, char **argv) {
	//Configuration:
	struct {
		std::string title = "Game1: Text/Tiles";
		glm::uvec2 size = glm::uvec2(480, 672);
	} config;

	//------------ initialization ------------

	//Initialize SDL library:
	SDL_Init(SDL_INIT_VIDEO);

	//Ask for an OpenGL context version 3.3, core profile, enable debug:
	SDL_GL_ResetAttributes();
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	//create window:
	SDL_Window *window = SDL_CreateWindow(
		config.title.c_str(),
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		config.size.x, config.size.y,
		SDL_WINDOW_OPENGL /*| SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI*/
	);

	if (!window) {
		std::cerr << "Error creating SDL window: " << SDL_GetError() << std::endl;
		return 1;
	}

	//Create OpenGL context:
	SDL_GLContext context = SDL_GL_CreateContext(window);

	if (!context) {
		SDL_DestroyWindow(window);
		std::cerr << "Error creating OpenGL context: " << SDL_GetError() << std::endl;
		return 1;
	}

	#ifdef _WIN32
	//On windows, load OpenGL extensions:
	if (!init_gl_shims()) {
		std::cerr << "ERROR: failed to initialize shims." << std::endl;
		return 1;
	}
	#endif

	//Set VSYNC + Late Swap (prevents crazy FPS):
	if (SDL_GL_SetSwapInterval(-1) != 0) {
		std::cerr << "NOTE: couldn't set vsync + late swap tearing (" << SDL_GetError() << ")." << std::endl;
		if (SDL_GL_SetSwapInterval(1) != 0) {
			std::cerr << "NOTE: couldn't set vsync (" << SDL_GetError() << ")." << std::endl;
		}
	}

	//Hide mouse cursor (note: showing can be useful for debugging):
	SDL_ShowCursor(SDL_DISABLE);

	//------------ opengl objects / game assets ------------

	//texture:
	GLuint tex = 0;
	glm::uvec2 tex_size = glm::uvec2(0,0);

	{ //load texture 'tex':
		std::vector< uint32_t > data;
		if (!load_png("textures.png", &tex_size.x, &tex_size.y, &data, LowerLeftOrigin)) {
			std::cerr << "Failed to load texture." << std::endl;
			exit(1);
		}
		//create a texture object:
		glGenTextures(1, &tex);
		//bind texture object to GL_TEXTURE_2D:
		glBindTexture(GL_TEXTURE_2D, tex);
		//upload texture data from data:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_size.x, tex_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
		//set texture sampling parameters:
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}

	//shader program:
	GLuint program = 0;
	GLuint program_Position = 0;
	GLuint program_TexCoord = 0;
	GLuint program_Color = 0;
	GLuint program_mvp = 0;
	GLuint program_tex = 0;
	{ //compile shader program:
		GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER,
			"#version 330\n"
			"uniform mat4 mvp;\n"
			"in vec4 Position;\n"
			"in vec2 TexCoord;\n"
			"in vec4 Color;\n"
			"out vec2 texCoord;\n"
			"out vec4 color;\n"
			"void main() {\n"
			"	gl_Position = mvp * Position;\n"
			"	color = Color;\n"
			"	texCoord = TexCoord;\n"
			"}\n"
		);

		GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER,
			"#version 330\n"
			"uniform sampler2D tex;\n"
			"in vec4 color;\n"
			"in vec2 texCoord;\n"
			"out vec4 fragColor;\n"
			"void main() {\n"
			"	fragColor = texture(tex, texCoord) * color;\n"
			"}\n"
		);

		program = link_program(fragment_shader, vertex_shader);

		//look up attribute locations:
		program_Position = glGetAttribLocation(program, "Position");
		if (program_Position == -1U) throw std::runtime_error("no attribute named Position");
		program_TexCoord = glGetAttribLocation(program, "TexCoord");
		if (program_TexCoord == -1U) throw std::runtime_error("no attribute named TexCoord");
		program_Color = glGetAttribLocation(program, "Color");
		if (program_Color == -1U) throw std::runtime_error("no attribute named Color");

		//look up uniform locations:
		program_mvp = glGetUniformLocation(program, "mvp");
		if (program_mvp == -1U) throw std::runtime_error("no uniform named mvp");
		program_tex = glGetUniformLocation(program, "tex");
		if (program_tex == -1U) throw std::runtime_error("no uniform named tex");
	}

	//vertex buffer:
	GLuint buffer = 0;
	{ //create vertex buffer
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
	}

	struct Vertex {
		Vertex(glm::vec2 const &Position_, glm::vec2 const &TexCoord_, glm::u8vec4 const &Color_) :
			Position(Position_), TexCoord(TexCoord_), Color(Color_) { }
		glm::vec2 Position;
		glm::vec2 TexCoord;
		glm::u8vec4 Color;
	};
	static_assert(sizeof(Vertex) == 20, "Vertex is nicely packed.");

	//vertex array object:
	GLuint vao = 0;
	{ //create vao and set up binding:
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		glVertexAttribPointer(program_Position, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0);
		glVertexAttribPointer(program_TexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2));
		glVertexAttribPointer(program_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLbyte *)0 + sizeof(glm::vec2) + sizeof(glm::vec2));
		glEnableVertexAttribArray(program_Position);
		glEnableVertexAttribArray(program_TexCoord);
		glEnableVertexAttribArray(program_Color);
	}

	//------------ sprite info ------------
	struct SpriteInfo {
		glm::vec2 min_uv = glm::vec2(0.0f);
		glm::vec2 max_uv = glm::vec2(1.0f);
		glm::vec2 rad = glm::vec2(1.0f);
	} grid00, grid01, grid10, grid11, grid20, grid21, grid30, grid31, grid40, grid41, 
		rock, player, treasure, text[4];

	grid00.min_uv = glm::vec2(0.0f, 0.83333f);
	grid00.max_uv = glm::vec2(0.2f, 1.0f);
	grid00.rad = glm::vec2(0.2f, 0.14286f);

	grid01.min_uv = glm::vec2(0.0f, 0.83333f);
	grid01.max_uv = glm::vec2(0.2f, 1.0f);
	grid01.rad = glm::vec2(0.14286f, 0.2f);

	grid10.min_uv = glm::vec2(0.2f, 0.83333f);
	grid10.max_uv = glm::vec2(0.4f, 1.0f);
	grid10.rad = glm::vec2(0.2f, 0.14286f);

	grid11.min_uv = glm::vec2(0.2f, 0.83333f);
	grid11.max_uv = glm::vec2(0.4f, 1.0f);
	grid11.rad = glm::vec2(0.14286f, 0.2f);

	grid20.min_uv = glm::vec2(0.4f, 0.83333f);
	grid20.max_uv = glm::vec2(0.6f, 1.0f);
	grid20.rad = glm::vec2(0.2f, 0.14286f);

	grid21.min_uv = glm::vec2(0.4f, 0.83333f);
	grid21.max_uv = glm::vec2(0.6f, 1.0f);
	grid21.rad = glm::vec2(0.14286f, 0.2f);

	grid30.min_uv = glm::vec2(0.6f, 0.83333f);
	grid30.max_uv = glm::vec2(0.8f, 1.0f);
	grid30.rad = glm::vec2(0.2f, 0.14286f);

	grid31.min_uv = glm::vec2(0.6f, 0.83333f);
	grid31.max_uv = glm::vec2(0.8f, 1.0f);
	grid31.rad = glm::vec2(0.14286f, 0.2f);

	grid40.min_uv = glm::vec2(0.8f, 0.83333f);
	grid40.max_uv = glm::vec2(1.0f, 1.0f);
	grid40.rad = glm::vec2(0.2f, 0.14286f);

	grid41.min_uv = glm::vec2(0.8f, 0.83333f);
	grid41.max_uv = glm::vec2(1.0f, 1.0f);
	grid41.rad = glm::vec2(0.14286f, 0.2f);

	rock.min_uv = glm::vec2(0.0f, 0.66667f);
	rock.max_uv = glm::vec2(0.2f, 0.83333f);
	rock.rad = glm::vec2(0.2f, 0.14286f);

	player.min_uv = glm::vec2(0.2f, 0.66667f);
	player.max_uv = glm::vec2(0.4f, 0.83333f);
	player.rad = glm::vec2(0.2f, 0.14286f);
	
	treasure.min_uv = glm::vec2(0.4f, 0.66667f);
	treasure.max_uv = glm::vec2(0.6f, 0.83333f);
	treasure.rad = glm::vec2(0.2f, 0.14286f);
	
	text[0].min_uv = glm::vec2(0.0f, 0.5f);
	text[0].max_uv = glm::vec2(1.0f, 0.66667f);
	text[0].rad = glm::vec2(1.0f, 0.14286f);

	text[1].min_uv = glm::vec2(0.0f, 0.33333f);
	text[1].max_uv = glm::vec2(1.0f, 0.5f);
	text[1].rad = glm::vec2(1.0f, 0.14286f);

	text[2].min_uv = glm::vec2(0.0f, 0.16667f);
	text[2].max_uv = glm::vec2(1.0f, 0.33333f);
	text[2].rad = glm::vec2(1.0f, 0.14286f);

	text[3].min_uv = glm::vec2(0.0f, 0.0f);
	text[3].max_uv = glm::vec2(1.0f, 0.16667f);
	text[3].rad = glm::vec2(1.0f, 0.14286f);

	//------------ pathing info ----------

	struct CellPath {
		float up = 10.0f;
		float down = -10.0f;
		float left = -10.0f;
		float right = 10.0f;
	} cell_paths[30];

	cell_paths[0].up = 0.87f;
	cell_paths[0].down = 0.84f;
	cell_paths[0].left = -0.812f; 

	cell_paths[1].up = 0.87f;

	cell_paths[2].up = 0.87f;

	cell_paths[3].up = 0.87f;
	cell_paths[3].right = 0.412f;

	cell_paths[4].up = 0.87f;
	cell_paths[4].left = 0.788f;
	cell_paths[4].right = 0.812f;

	cell_paths[5].up = 0.583f;
	cell_paths[5].left = -0.812f;
	cell_paths[5].right = -0.788f;

	cell_paths[6].down = 0.559f;
	cell_paths[6].left = -0.412f;
	cell_paths[6].right = -0.388f;

	cell_paths[7].left = -0.012f;

	cell_paths[8].right = 0.412f;

	cell_paths[9].left = 0.788f;
	cell_paths[9].right = 0.812f;

	cell_paths[10].down = 0.274f;
	cell_paths[10].left = -0.812f;

	cell_paths[11].up = 0.298f;
	cell_paths[11].down = 0.274f;

	cell_paths[12].right = 0.012f;

	cell_paths[13].down = 0.274f;
	cell_paths[13].left = 0.388f;

	cell_paths[14].down = 0.274f;
	cell_paths[14].right = 0.812f;

	cell_paths[15].up = 0.012f;
	cell_paths[15].left = -0.812f;
	cell_paths[15].right = -0.788f;

	cell_paths[16].up = 0.012f;
	cell_paths[16].left = -0.412f;

	cell_paths[17].down = -0.012f;

	cell_paths[18].up = 0.012f;

	cell_paths[19].up = 0.012f;
	cell_paths[19].right = 0.812f;

	cell_paths[20].left = -0.812f;
	cell_paths[20].right = -0.788f;

	cell_paths[21].left = -0.412f;
	cell_paths[21].right = -0.388f;

	cell_paths[22].up = -0.274f;
	cell_paths[22].left = -0.012f;
	cell_paths[22].right = 0.012f;

	cell_paths[23].left = 0.388f;
	cell_paths[23].right = 0.412f;

	cell_paths[24].down = -0.298f;
	cell_paths[24].left = 0.788f;
	cell_paths[24].right = 0.812f;

	cell_paths[25].down = -0.60f;
	cell_paths[25].left = -0.812f;

	cell_paths[26].down = -0.60f;

	cell_paths[27].down = -0.60f;

	cell_paths[28].down = -0.60f;

	cell_paths[29].up = -0.559f;
	cell_paths[29].down = -0.60f;
	cell_paths[29].right = 0.812f;

	//------------ game state ------------

	int current_text = 0;
	glm::vec2 player_pos = glm::vec2(0.0f, 0.28571f);
	float player_speed = 1.0f;

	srand((unsigned)time(0));
	int treasure_rock = rand() % 5;
	glm::vec2 treasure_pos;
	bool treasure_found = false;
	switch (treasure_rock) {
		case 0:
			treasure_pos = glm::vec2(0.8f, 0.85714f);
			break;
		case 1:
			treasure_pos = glm::vec2(-0.8f, 0.57143f);
			break;
		case 2:
			treasure_pos = glm::vec2(-0.8f, 0.0f);
			break;
		case 3:
			treasure_pos = glm::vec2(0.0f, -0.28571f);
			break;
		case 4:
			treasure_pos = glm::vec2(0.8f, -0.57143f);
			break;
	}

	bool cells_visited[30] = {};
	bool rocks_mined[5] = {};
	int current_cell = 12;

	//------------ game loop ------------

	bool should_quit = false;
	float pi = 3.14159265f;
	while (true) {
		
		auto current_time = std::chrono::high_resolution_clock::now();
		static auto previous_time = current_time;
		float elapsed = std::chrono::duration< float >(current_time - previous_time).count();
		previous_time = current_time;

		static SDL_Event evt;
		while (SDL_PollEvent(&evt) == 1) {
			//handle input:
			if (evt.type == SDL_KEYDOWN) {
				switch (evt.key.keysym.sym) {
					case SDLK_ESCAPE:
						should_quit = true;
						break;
					case SDLK_UP:
						if (!treasure_found && player_pos.y < cell_paths[current_cell].up) {
							player_pos.y += player_speed * elapsed;
						}
						break;
					case SDLK_DOWN:
						if (!treasure_found && player_pos.y > cell_paths[current_cell].down) {
							player_pos.y -= player_speed * elapsed;
						}
						break;
					case SDLK_RIGHT:
						if (!treasure_found && player_pos.x < cell_paths[current_cell].right) {
							player_pos.x += player_speed * elapsed;
						}
						break;
					case SDLK_LEFT:
						if (!treasure_found && player_pos.x > cell_paths[current_cell].left) {
							player_pos.x -= player_speed * elapsed;
						}
						break;
					case SDLK_SPACE:
						if (!treasure_found) {
							if (current_cell == 4) {
								rocks_mined[0] = true;
								if (treasure_rock == 0) {
									treasure_found = true;
								}
							} else if (current_cell == 5) {
								rocks_mined[1] = true;
								if (treasure_rock == 1) {
									treasure_found = true;
								}
							} else if (current_cell == 15) {
								rocks_mined[2] = true;
								if (treasure_rock == 2) {
									treasure_found = true;
								}
							} else if (current_cell == 22) {
								rocks_mined[3] = true;
								if (treasure_rock == 3) {
									treasure_found = true;
								}
							} else if (current_cell == 29) {
								rocks_mined[4] = true;
								if (treasure_rock == 4) {
									treasure_found = true;
								}
							}
						}
						break;
				}
			} else if (evt.type == SDL_QUIT) {
				should_quit = true;
				break;
			}
		}
		if (should_quit) break;

		{ //update game state:
			int current_cell_y = (int)((1.85714 - (player_pos.y + 0.85714))*3.5f);
			int current_cell_x = (int)((player_pos.x + 1.0f)*2.5f);
			current_cell = current_cell_y * 5 + current_cell_x;
			
			cells_visited[current_cell] = true;
			if (current_cell == 4) {
				if (rocks_mined[0]) {
					if (treasure_rock == 0) {
						current_text = 3;
					} else {
						current_text = 2;
					}
				} else {
					current_text = 1;
				}
			} else if (current_cell == 5) {
				if (rocks_mined[1]) {
					if (treasure_rock == 1) {
						current_text = 3;
					} else {
						current_text = 2;
					}
				} else {
					current_text = 1;
				}
			} else if (current_cell == 15) {
				if (rocks_mined[2]) {
					if (treasure_rock == 2) {
						current_text = 3;
					} else {
						current_text = 2;
					}
				} else {
					current_text = 1;
				}
			} else if (current_cell == 22) {
				if (rocks_mined[3]) {
					if (treasure_rock == 3) {
						current_text = 3;
					} else {
						current_text = 2;
					}
				} else {
					current_text = 1;
				} 
			} else if (current_cell == 29) {
				if (rocks_mined[4]) {
					if (treasure_rock == 4) {
						current_text = 3;
					} else {
						current_text = 2;
					}
				} else {
					current_text = 1;
				}
			} else {
				current_text = 0;
			}
		}

		//draw output:
		glClearColor(0.0, 0.0, 0.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		{ //draw game state:
			std::vector< Vertex > verts;

			auto draw_sprite = [&verts](SpriteInfo const &sprite, glm::vec2 const &at, float angle = 0.0f) {
				glm::vec2 min_uv = sprite.min_uv;
				glm::vec2 max_uv = sprite.max_uv;
				glm::vec2 rad = sprite.rad;
				glm::u8vec4 tint = glm::u8vec4(0xff, 0xff, 0xff, 0xff);
				glm::vec2 right = glm::vec2(std::cos(angle), std::sin(angle));
				glm::vec2 up = glm::vec2(-right.y, right.x);

				verts.emplace_back(at + right * -rad.x + up * -rad.y, glm::vec2(min_uv.x, min_uv.y), tint);
				verts.emplace_back(verts.back());
				verts.emplace_back(at + right * -rad.x + up * rad.y, glm::vec2(min_uv.x, max_uv.y), tint);
				verts.emplace_back(at + right * rad.x + up * -rad.y, glm::vec2(max_uv.x, min_uv.y), tint);
				verts.emplace_back(at + right * rad.x + up * rad.y, glm::vec2(max_uv.x, max_uv.y), tint);
				verts.emplace_back(verts.back());
			};

			if (cells_visited[0]) {
				draw_sprite(grid00, glm::vec2(-0.8f, 0.85714f), pi/2.0f * 0.0f);
			}
			if (cells_visited[1]) {
				draw_sprite(grid31, glm::vec2(-0.4f, 0.85714f), pi/2.0f * 1.0f);
			}
			if (cells_visited[2]) {
				draw_sprite(grid31, glm::vec2(0.0f, 0.85714f), pi/2.0f * 1.0f);
			}
			if (cells_visited[3]) {
				draw_sprite(grid21, glm::vec2(0.4f, 0.85714f), pi/2.0f * 1.0f);
			}
			if (cells_visited[4]) {
				draw_sprite(grid01, glm::vec2(0.8f, 0.85714f), pi/2.0f * 3.0f);
			}

			if (cells_visited[5]) {
				draw_sprite(grid01, glm::vec2(-0.8f, 0.57143f), pi/2.0f * 3.0f);
			}
			if (cells_visited[6]) {
				draw_sprite(grid01, glm::vec2(-0.4f, 0.57143f), pi/2.0f * 1.0f);
			}
			if (cells_visited[7]) {
				draw_sprite(grid30, glm::vec2(0.0f, 0.57143f), pi/2.0f * 2.0f);
			}
			if (cells_visited[8]) {
				draw_sprite(grid30, glm::vec2(0.4f, 0.57143f), pi/2.0f * 0.0f);
			}
			if (cells_visited[9]) {
				draw_sprite(grid10, glm::vec2(0.8f, 0.57143f), pi/2.0f * 0.0f);
			}
			
			if (cells_visited[10]) {
				draw_sprite(grid21, glm::vec2(-0.8f, 0.28571f), pi/2.0f * 3.0f);
			}
			if (cells_visited[11]) {
				draw_sprite(grid11, glm::vec2(-0.4f, 0.28571f), pi/2.0f * 1.0f);
			}
			if (cells_visited[12]) {
				draw_sprite(grid30, glm::vec2(0.0f, 0.28571f), pi/2.0f * 0.0f);
			}
			if (cells_visited[13]) {
				draw_sprite(grid21, glm::vec2(0.4f, 0.28571f), pi/2.0f * 3.0f);
			}
			if (cells_visited[14]) {
				draw_sprite(grid20, glm::vec2(0.8f, 0.28571f), pi/2.0f * 0.0f);
			}

			if (cells_visited[15]) {
				draw_sprite(grid01, glm::vec2(-0.8f, 0.0f), pi/2.0f * 3.0f);
			}
			if (cells_visited[16]) {
				draw_sprite(grid20, glm::vec2(-0.4f, 0.0f), pi/2.0f * 2.0f);
			}
			if (cells_visited[17]) {
				draw_sprite(grid31, glm::vec2(0.0f, 0.0f), pi/2.0f * 3.0f);
			}
			if (cells_visited[18]) {
				draw_sprite(grid31, glm::vec2(0.4f, 0.0f), pi/2.0f * 1.0f);
			}
			if (cells_visited[19]) {
				draw_sprite(grid21, glm::vec2(0.8f, 0.0f), pi/2.0f * 1.0f);
			}
			
			if (cells_visited[20]) {
				draw_sprite(grid10, glm::vec2(-0.8f, -0.28571f), pi/2.0f * 0.0f);
			}
			if (cells_visited[21]) {
				draw_sprite(grid10, glm::vec2(-0.4f, -0.28571f), pi/2.0f * 0.0f);
			}
			if (cells_visited[22]) {
				draw_sprite(grid01, glm::vec2(0.0f, -0.28571f), pi/2.0f * 3.0f);
			}
			if (cells_visited[23]) {
				draw_sprite(grid10, glm::vec2(0.4f, -0.28571f), pi/2.0f * 0.0f);
			}
			if (cells_visited[24]) {
				draw_sprite(grid01, glm::vec2(0.8f, -0.28571f), pi/2.0f * 1.0f); 
			}

			if (cells_visited[25]) {
				draw_sprite(grid21, glm::vec2(-0.8f, -0.57143f), pi/2.0f * 3.0f);
			}
			if (cells_visited[26]) {
				draw_sprite(grid31, glm::vec2(-0.4f, -0.57143f), pi/2.0f * 3.0f);
			}
			if (cells_visited[27]) {
				draw_sprite(grid31, glm::vec2(0.0f, -0.57143f), pi/2.0f * 3.0f);
			}
			if (cells_visited[28]) {
				draw_sprite(grid31, glm::vec2(0.4f, -0.57143f), pi/2.0f * 3.0f);
			}
			if (cells_visited[29]) {
				draw_sprite(grid00, glm::vec2(0.8f, -0.57143f), pi/2.0f * 2.0f); 
			}
		 
			if (cells_visited[4] && !rocks_mined[0]) {
				draw_sprite(rock, glm::vec2(0.8f, 0.85714f)); 
			}
			if (cells_visited[5] && !rocks_mined[1]) {
				draw_sprite(rock, glm::vec2(-0.8f, 0.57143f)); 
			}
			if (cells_visited[15] && !rocks_mined[2]) {
				draw_sprite(rock, glm::vec2(-0.8f, 0.0f)); 
			}
			if (cells_visited[22] && !rocks_mined[3]) {
				draw_sprite(rock, glm::vec2(0.0f, -0.28571f)); 
			}
			if (cells_visited[29] && !rocks_mined[4]) {
				draw_sprite(rock, glm::vec2(0.8f, -0.57143f)); 
			}

			draw_sprite(player, player_pos);
			draw_sprite(text[current_text], glm::vec2(0.0f, -0.85714f)); 
			
			if (treasure_found) {
				draw_sprite(treasure, treasure_pos);
			}

			glBindBuffer(GL_ARRAY_BUFFER, buffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * verts.size(), &verts[0], GL_STREAM_DRAW);

			glUseProgram(program);
			glUniform1i(program_tex, 0);
			glm::mat4 mvp = glm::mat4(1.0f);
			glUniformMatrix4fv(program_mvp, 1, GL_FALSE, glm::value_ptr(mvp));

			glBindTexture(GL_TEXTURE_2D, tex);
			glBindVertexArray(vao);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, verts.size());
		}

		SDL_GL_SwapWindow(window);
	}


	//------------ teardown ------------

	SDL_GL_DeleteContext(context);
	context = 0;

	SDL_DestroyWindow(window);
	window = NULL;

	return 0;
}

static GLuint compile_shader(GLenum type, std::string const &source) {
	GLuint shader = glCreateShader(type);
	GLchar const *str = source.c_str();
	GLint length = source.size();
	glShaderSource(shader, 1, &str, &length);
	glCompileShader(shader);
	GLint compile_status = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		std::cerr << "Failed to compile shader." << std::endl;
		GLint info_log_length = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetShaderInfoLog(shader, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		glDeleteShader(shader);
		throw std::runtime_error("Failed to compile shader.");
	}
	return shader;
}

static GLuint link_program(GLuint fragment_shader, GLuint vertex_shader) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);
	GLint link_status = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		std::cerr << "Failed to link shader program." << std::endl;
		GLint info_log_length = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_log_length);
		std::vector< GLchar > info_log(info_log_length, 0);
		GLsizei length = 0;
		glGetProgramInfoLog(program, info_log.size(), &length, &info_log[0]);
		std::cerr << "Info log: " << std::string(info_log.begin(), info_log.begin() + length);
		throw std::runtime_error("Failed to link program");
	}
	return program;
}
