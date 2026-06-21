#version 330 core

// Atributos de fragmentos recebidos como entrada ("in") pelo Fragment Shader.
// Neste exemplo, este atributo foi gerado pelo rasterizador como a
// interpolacao da posicao global e a normal de cada vertice, definidas em
// "shader_vertex.glsl" e "main.cpp".
in vec4 position_world;
in vec4 normal;

// Posicao do vertice atual no sistema de coordenadas local do modelo.
in vec4 position_model;

// Coordenadas de textura obtidas do arquivo OBJ (se existirem!)
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
#define DEBUG_CUBE 6
#define PROJECTILE 5
uniform int object_id;
uniform int cube_colliding;
uniform vec4 debug_color;

// Parametros da axis-aligned bounding box (AABB) do modelo
uniform vec4 bbox_min;
uniform vec4 bbox_max;

// Variaveis para acesso das imagens de textura
uniform sampler2D TextureImage0;
uniform sampler2D TextureImage1;
uniform sampler2D TextureImage2;

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

    // O fragmento atual e coberto por um ponto que percente a superficie de um
    // dos objetos virtuais da cena. Este ponto, p, possui uma posicao no
    // sistema de coordenadas global (World coordinates). Esta posicao e obtida
    // atraves da interpolacao, feita pelo rasterizador, da posicao de cada
    // vertice.
    vec4 p = position_world;

    // Normal do fragmento atual, interpolada pelo rasterizador a partir das
    // normais de cada vertice.
    vec4 n = normalize(normal);

    // Vetor que define o sentido da fonte de luz em relacao ao ponto atual.
    vec4 l = normalize(vec4(1.0,1.0,0.0,0.0));

    // Vetor que define o sentido da camera em relacao ao ponto atual.
    vec4 v = normalize(camera_position - p);

    // Coordenadas de textura U e V
    float U = 0.0;
    float V = 0.0;

    // Coeficiente de refletancia difusa
    vec3 Kd0 = vec3(0.7, 0.7, 0.7);
    float alpha = 1.0;

    if ( object_id == SPHERE )
    {
        float pulse = 0.75 + 0.25 * max(0.0, n.y);
        Kd0 = pulse * vec3(0.55, 0.95, 0.70);
        alpha = 1.0;
    }
    else if ( object_id == BUNNY )
    {
        // PREENCHA AQUI as coordenadas de textura do coelho, computadas com
        // projecao planar XY em COORDENADAS DO MODELO. Utilize como referencia
        // o slides 99-104 do documento Aula_20_Mapeamento_de_Texturas.pdf,
        // e tambem use as variaveis min*/max* definidas abaixo para normalizar
        // as coordenadas de textura U e V dentro do intervalo [0,1]. Para
        // tanto, veja por exemplo o mapeamento da variavel 'p_v' utilizando
        // 'h' no slides 158-160 do documento Aula_20_Mapeamento_de_Texturas.pdf.
        // Veja tambem a Questao 4 do Questionario 4 no Moodle.

        float minx = bbox_min.x;
        float maxx = bbox_max.x;

        float miny = bbox_min.y;
        float maxy = bbox_max.y;

        float minz = bbox_min.z;
        float maxz = bbox_max.z;

        U = (position_model.x - minx) / (maxx - minx);
        V = (position_model.y - miny) / (maxy - miny);

        // Obtemos a refletancia difusa a partir da leitura da imagem TextureImage0
        vec4 tex_color = texture(TextureImage0, vec2(U,V));
        Kd0 = tex_color.rgb;
        alpha = tex_color.a;
    }
    else if ( object_id == PLANE )
    {
        // Coordenadas de textura do plano, obtidas do arquivo OBJ.
        U = texcoords.x;
        V = texcoords.y;

        // Obtemos a refletancia difusa a partir da leitura da imagem TextureImage1
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
        Kd0 = vec3(0.46, 0.28, 0.12);
        alpha = 1.0;
    }

    // Equacao de Iluminacao
    float lambert = max(0,dot(n,l));

    color.rgb = Kd0 * (lambert + 0.1);

    // NOTE: Se voce quiser fazer o rendering de objetos transparentes, e
    // necessario:
    // 1) Habilitar a operacao de "blending" de OpenGL logo antes de realizar o
    //    desenho dos objetos transparentes, com os comandos abaixo no codigo C++:
    //      glEnable(GL_BLEND);
    //      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // 2) Realizar o desenho de todos objetos transparentes *apos* ter desenhado
    //    todos os objetos opacos; e
    // 3) Realizar o desenho de objetos transparentes ordenados de acordo com
    //    suas distancias para a camera (desenhando primeiro objetos
    //    transparentes que estao mais longe da camera).
    // Alpha default = 1 = 100% opaco = 0% transparente
    color.a = alpha;

    if (color.a < 0.5)
        discard;

    // Forcamos alpha = 1 para evitar halos brancos (fringing) nas bordas
    // provocados pela interpolacao linear com o fundo transparente.
    color.a = 1.0;

    // Cor final com correcao gamma, considerando monitor sRGB.
    color.rgb = pow(color.rgb, vec3(1.0,1.0,1.0)/2.2);
}
