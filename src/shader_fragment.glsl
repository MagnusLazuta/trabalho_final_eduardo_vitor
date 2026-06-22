#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
in vec4 position_world;
in vec4 normal;

// Posicao do vertice atual no sistema de coordenadas local do modelo.
in vec4 position_model;

// Coordenadas de textura obtidas do modelo
in vec2 texcoords;

// Matrizes computadas no codigo C++ e enviadas para a GPU
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Identificador que define qual objeto esta sendo desenhado no momento
#define SPHERE 0
#define BUNNY  1
#define PLANE  2
#define SCENARIO 3
#define PLAYER_CUBE 4
#define PROJECTILE 5
#define DEBUG_CUBE 6
<<<<<<< HEAD
#define PLAYER 7
#define ENEMY 8
=======
#define ENEMY 7
#define EFFECT 8
>>>>>>> main
uniform int object_id;
uniform int cube_colliding;
uniform vec4 debug_color;
uniform vec3 object_tint;
uniform float effect_alpha;

// Parametros da axis-aligned bounding box (AABB) do modelo
uniform vec4 bbox_min;
uniform vec4 bbox_max;

// Variaveis para acesso das imagens de textura
uniform sampler2D TextureImage0;
uniform sampler2D TextureImage1;
uniform sampler2D TextureImage2;

// Textura do modelo do jogador
uniform sampler2D playerTexture;
uniform int hasPlayerTexture;

// Propriedades do material
uniform vec3 materialDiffuse;
uniform vec3 materialSpecular;
uniform vec3 materialAmbient;
uniform float materialShininess;

// O valor de saida ("out") de um Fragment Shader e a cor final do fragmento.
out vec4 color;

// Constantes
#define M_PI   3.14159265358979323846
#define M_PI_2 1.57079632679489661923

void main()
{
    // Obtemos a posicao da camera utilizando a inversa da matriz que define o
    // sistema de coordenadas da camera.
    vec4 origin = vec4(0.0, 0.0, 0.0, 1.0);
    vec4 camera_position = inverse(view) * origin;

    // Posicao do fragmento atual no sistema de coordenadas global (World)
    vec4 p = position_world;

    // Normal do fragmento atual
    vec4 n = normalize(normal);

    // Vetor que define o sentido da fonte de luz em relacao ao ponto atual.
    vec4 l = normalize(vec4(1.0,1.0,0.0,0.0));

    // Vetor que define o sentido da camera em relacao ao ponto atual.
    vec4 v = normalize(camera_position - p);

    // Coordenadas de textura U e V
    float U = texcoords.x;
    float V = texcoords.y;

    // Coeficiente de refletancia difusa
    vec3 Kd0 = materialDiffuse;
    float alpha = 1.0;

    if ( object_id == SPHERE )
    {
        float pulse = 0.75 + 0.25 * max(0.0, n.y);
        Kd0 = pulse * object_tint.rgb;
        alpha = 1.0;
    }
    else if ( object_id == BUNNY )
    {
        float minx = bbox_min.x;
        float maxx = bbox_max.x;

        float miny = bbox_min.y;
        float maxy = bbox_max.y;

        float minz = bbox_min.z;
        float maxz = bbox_max.z;

        U = (position_model.x - minx) / (maxx - minx);
        V = (position_model.y - miny) / (maxy - miny);

        vec4 tex_color = texture(TextureImage0, vec2(U,V));
        Kd0 = tex_color.rgb;
        alpha = tex_color.a;
    }
    else if ( object_id == PLANE )
    {
        vec4 tex_color = texture(TextureImage1, vec2(U,V));
        Kd0 = tex_color.rgb;
        alpha = tex_color.a;
    }
    else if ( object_id == SCENARIO )
    {
        vec4 tex_color = texture(TextureImage0, texcoords);
        Kd0 = tex_color.rgb;
        alpha = tex_color.a;
    }
    else if ( object_id == PLAYER_CUBE )
    {
        Kd0 = (cube_colliding == 1) ? vec3(0.95, 0.20, 0.20) : vec3(0.20, 0.85, 0.30);
    }
    else if ( object_id == DEBUG_CUBE )
    {
        Kd0 = debug_color.rgb;
        alpha = debug_color.a;
    }
    else if ( object_id == PROJECTILE )
    {
        Kd0 = vec3(0.65, 0.85, 1.0);
        alpha = 1.0;
    }
    else if ( object_id == PLAYER )
    {
        // Para o jogador, usa textura se disponível
        if (hasPlayerTexture == 1) {
            vec4 tex_color = texture(playerTexture, texcoords);
            Kd0 = tex_color.rgb;
            alpha = tex_color.a;
        } else {
            // Cores padrão para o jogador sem textura
            Kd0 = vec3(0.2, 0.6, 0.8);
        }
    }
    else if ( object_id == ENEMY )
    {
        vec4 tex_color = texture(TextureImage0, texcoords);
        Kd0 = tex_color.rgb;
        alpha = tex_color.a;
    }
    else if ( object_id == EFFECT )
    {
        vec4 tex_color = texture(TextureImage0, texcoords);
        float intensity = max(tex_color.r, max(tex_color.g, tex_color.b));
        Kd0 = max(intensity, 0.20) * object_tint.rgb;
        alpha = tex_color.a * effect_alpha;
    }

    // Equacao de Iluminacao
    float lambert = max(0,dot(n,l));

    color.rgb = Kd0 * (lambert + 0.1);
    if ( object_id == PROJECTILE )
    {
        color.rgb = Kd0 * (0.65 + 0.35 * lambert) + vec3(0.12, 0.16, 0.20);
    }
    else if ( object_id == EFFECT )
    {
        color.rgb = Kd0 * (0.55 + 0.45 * lambert);
    }

    // Alpha
    color.a = alpha;

    if (object_id == EFFECT)
    {
        if (color.a < 0.05)
            discard;
    }
    else if (color.a < 0.5)
        discard;

<<<<<<< HEAD
    // Forcamos alpha = 1 para evitar halos brancos
    color.a = 1.0;
=======
    // Forcamos alpha = 1 para evitar halos brancos (fringing) nas bordas
    // provocados pela interpolacao linear com o fundo transparente.
    if (object_id != DEBUG_CUBE && object_id != EFFECT)
        color.a = 1.0;
>>>>>>> main

    // Cor final com correcao gamma
    color.rgb = pow(color.rgb, vec3(1.0,1.0,1.0)/2.2);
}