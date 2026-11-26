# Validacion local — 23 de noviembre de 2026

Entorno comprobado: Linux x86_64, GCC 16.2, CMake 4.5, Python 3.14 y SDL 2.32.74. ROM USA PRG0 con SHA-256 `6ea0777ca520ba7ad7b0ea0f6452140d59aa0b1dffe5045cb1e8020dde10c267`. Se usaron las revisiones de `dependencies.json` y la PPU LLE de NESRecomp.

## Comprobaciones reproducibles

```sh
python3 -m unittest discover -s tests -v
python3 tools/verify.py
python3 tools/verify_graphics.py
python3 tools/verify_widescreen.py
ctest --test-dir build --output-on-failure
```

- **20 pruebas Python** con datos sintéticos: importación y arranque, idioma del selector, integridad de la ROM, exclusión de archivos privados, lectura de capturas y copia del arte preparado. `render_math` comprueba con hojas sintéticas los conversores de resource packs.
- **600 fotogramas en 1-1**, entrando desde el título sin cambiar RAM: coinciden las trazas, la imagen de la PPU y el audio. La compilación estándar usa el intérprete. El mismo script compara código nativo e intérprete al ejecutarse sobre una compilación nativa local.
- **Guardar en el fotograma 300 y continuar hasta el 600**: al cargar, todos los fotogramas restantes coinciden con la ejecucion continua.
- **Mapas de los ocho mundos**, 450 fotogramas por mundo: coinciden las trazas y se captura su presentación Remastered. La prueba selecciona el mundo de forma asistida.
- **ROM alterada**: se rechaza una imagen con un byte cambiado en el programa y otra con un byte cambiado en el área gráfica.
- **Cobertura nativa de la ruta corta**: el porcentaje exacto y los contadores de ciclos figuran en `verification/checks/result.json`. El motor redondea su porcentaje en consola; esa cifra describe la ruta comprobada, no todas las instrucciones posibles.
- **Paquete de fuentes**: se verifica el contenido del ZIP y se compila una copia limpia sin ROM. La misma copia llega a 1-1 después de recibir la ROM válida.

Los logs, PNG, WAV, estados y `result.json` de estas comprobaciones estan en `verification/checks/`, excluido de Git.

## Interfaz y entrada

El ejecutable abre el selector de ROM al arrancar sin argumentos ni una ruta válida guardada. En Linux se ejercitó el arranque con ventanas SDL offscreen y respuestas simuladas del selector del sistema. El ejecutable rechazó una ROM con CHR alterada, aceptó la original, recordó su ruta al abrirse de nuevo y salió al cancelar. El asistente de `play.sh` y `play.cmd` admite `.nes` y `.zip`.

Se comprobó el cambio de inglés a español con el teclado, la carga del idioma al volver a abrir el juego y el cambio a inglés con un mando virtual SDL. La prueba abrió el selector de personaje, los controles, Pruebas, la pausa, las partidas guardadas y el rebobinado en ambos idiomas. También comprobó los títulos del selector de ROM con respuestas simuladas.

Las capturas en `verification/intro/ui-v2/` muestran el menu principal, el selector teatral, ajustes, controles, partidas guardadas y rebobinado. La prueba de ventana X11 confirma el cambio de personaje, la persistencia tras aceptar, la salida sin aplicar al cancelar, la asignacion de una tecla y el inicio de 1-1. Dentro de 1-1 guarda y carga una ranura de prueba y reanuda desde rebobinado. Tambien cambia el selector entre pantalla completa de 1920 × 1080 y ventana 4:3 de 1024 × 768.

Un mando virtual de SDL recorre la misma ruta de 600 fotogramas a traves de las asignaciones reales de botones: Start, cruceta, A y X. Entra a 1-1, corre y salta. Para repetirlo en Linux con una compilacion local:

```sh
cd build
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy NESRECOMP_NO_LAUNCHER=1 \
  ./smb3 ../private/baserom.nes --hidden --exit-after 600 --uncapped \
  --virtual-pad ../tests/fixtures/controller.txt --skip-selection --no-save
```

La captura de la interfaz de juego a 600 fotogramas esta en `verification/virtual-pad_00600.png`, y la del lanzador en `verification/launcher.png`.

## Rutas largas y alcance real

Se intentaron dos grabaciones publicas de FCEUX con la ROM correcta:

- [All levels, TASVideos #2835](https://tasvideos.org/2835M), de Lord_Tom y Tompa. Se avanzo naturalmente por los mundos 1, 2 y 3, pero mas adelante los controles grabados dejaron de corresponderse con la situacion del juego. La ejecucion fue detenida tras observar la desincronizacion.
- [Warps, TASVideos #3922](https://tasvideos.org/3922M), de Lord_Tom, Maru y Tompa. Se ejecutaron 40000 fotogramas y se llego al castillo final del mundo 8, pero la grabacion tambien perdio sincronizacion antes de completar la partida.

**Ninguna de esas reproducciones se cuenta como una partida completa verificada.** No se ha establecido la causa exacta del desfase con FCEUX. Esto queda pendiente y no debe ocultarse al describir el estado del proyecto.

Las direcciones ejecutadas ampliaron el perfil nativo opcional a 19878 entradas. Esa compilación usa las semillas para traducir los bancos PRG y deja al intérprete las rutas sin código generado.

Tras ampliar el perfil, se repitieron las comprobaciones cortas y se compararon 2500 fotogramas del castillo del mundo 8 desde un estado de prueba: las trazas y la imagen final coinciden exactamente entre nativo e interprete, sin instrucciones del juego interpretadas en ese tramo. Esto comprueba la recompilacion de ese tramo, no una victoria contra Bowser.

Tambien se compilo el nucleo de referencia TriCNES incluido en las pruebas de NESRecomp. Tras 3000 fotogramas con las mismas entradas, coinciden exactamente el contador de ciclos, RAM, CIRAM, OAM, paletas, RAM de cartucho y los indices de color de toda la imagen entre ese nucleo y el ejecutable nativo. Esta comparacion independiente cubre ese punto de la ruta; no establece la causa del desfase posterior de las grabaciones de FCEUX. TriCNES se utilizo solo como herramienta local de comprobacion y no se enlaza en el ejecutable entregado.

## Limites de la comprobacion

Las pruebas de juego descritas aquí se realizaron en Linux. El workflow compila y empaqueta para Windows y Linux, y comprueba el arranque del ejecutable empaquetado con datos de prueba sintéticos. La validación con la ROM se hace localmente. Las pruebas con mandos físicos, altavoces y dos mandos reales siguen pendientes. La salida de audio se verificó como PCM y con dispositivo dummy.

## Presentacion 0.2 y personajes NES

La ROM controla poses, tiempos, posición, parpadeo y prioridad de los sprites. La presentación remasterizada recolorea los sprites y compone los fondos por ambiente. La instalación local de SMB3-mastered v0.9 incorpora las hojas de Toad y Toadette, además de las secuencias de bloques `?`, ladrillos y monedas en los niveles horizontales.

- **Cuatro personajes**, 450 fotogramas por personaje: los hashes de CPU/memoria/hardware, la imagen de la PPU y el audio coinciden entre las cuatro apariencias panoramicas. Las cuatro presentaciones son distintas. En ancho nativo, Remastered y clasico conservan las mismas trazas.
- **Los mismos cuatro personajes en el mapa**: cuatro presentaciones distintas, con imagen de hardware, audio y trazas identicos al mapa clasico. Apertura y cierre del inventario con B, comprobando su estado en RAM y capturando ambas pantallas.
- **Siete poderes por cuatro personajes**, 28 capturas: cada poder se selecciona en `World_Map_Power` antes de entrar en 1-1 para que el propio juego inicialice el traje y su paleta. Se comprueban ambos datos y la igualdad de las trazas entre personajes al continuar desde ese estado. Esto cubre presentacion y continuacion, no conseguir los objetos jugando una campana.
- **Tres aspectos**: 256x240 nativo, 426x240 en 16:9 y 560x240 para una ventana adaptativa de 1680x720. Se mantiene el tamano de los pixeles de los sprites en el espacio del juego.
- **Con solo la ROM**: una copia temporal del ejecutable presenta a Mario y Luigi con los fondos generados. El selector muestra las apariencias disponibles en esa instalación.
- **Comprobación C++**: decodificación de bitplanes, mezcla, sombras, paletas, lectura de poses desde una hoja, selección de ambientes e identificación de los metatiles animados.
- **Coincidencia con el pack aportado**: los 1464 pixeles opacos de las seis poses base (Toad/Toadette pequeno, grande y fuego) coinciden exactamente con sus PNG fuente. Se compararon capturas del ejecutable, no solo las plantillas.
- **Movimiento de Toad/Toadette**: catorce secuencias desde los siete estados de poder, con caminar a ambos lados, agacharse y saltar. Otras cuatro secuencias comprueban los giros pequenos/grandes y los cambios de paleta de la estrella, activada de forma asistida para QA. Se revisaron capturas ampliadas en `verification/graphics/movement-detail.png` y `star-review.png`.
- **Poses aéreas**. En 1-1 se comprueban las secuencias de vuelo vertical y planeo de Mapache y Tanooki y el lanzamiento aéreo con Fuego y Martillo. Las cuatro apariencias conservan la misma simulación. En `verification/flight/` se pueden revisar las poses de los tres personajes con atlas local.
- **Mario original**: una comparacion local de los 68 pixeles de color rojo/piel en la pose pequena de prueba coincide exactamente con los RGB de la imagen original; tambien se inspeccionaron los contornos visualmente.
- **Interfaz**: selector revisado en ventana X11, confirmacion con Enter y con Start de un mando virtual, menu de ajustes y cambio de tamano a 1280x720. Se conservaron capturas privadas en `verification/graphics/`. La captura de interfaz del backend SDL2-compatible offscreen no esta disponible en este equipo; se utilizo una captura de la ventana X11 para esa revision.
- **Ventana con mando virtual**: 600 fotogramas hasta 1-1, audio producido y 60.8 fps en la prueba sin limite con el backend offscreen de esta maquina. No es una medicion de rendimiento en otros equipos o en 4K.

Los resultados reproducibles del ejecutable se guardan en `verification/graphics/result.json` y `verification/checks/result.json`, con su SHA-256. La ROM, las hojas de sprites, las grabaciones y las capturas permanecen en directorios locales.

La revisión visual cargó salas reales de pradera, fortaleza, desierto, agua, montaña, nieve, tuberías y el convoy del mundo 8. Para llegar a las salas se eligieron nodos del mapa de forma asistida. Las capturas con el pack y con solo la ROM están en `verification/visual-audit/`. Durante 600 fotogramas de la misma ruta en modo clásico, el ejecutable que carga la ROM y la compilación nativa anterior produjeron la misma traza, imagen de la PPU y audio.

El ajuste de fondos se comparó en nueve escenarios, incluido el tramo de cielo con desplazamiento automático. Los hashes del juego coinciden con los de la compilación anterior. En el agua y el convoy del mundo 8 se comprobaron ambos cuadros de los atlas del pack y sus cadencias de 2 y 5 cuadros por segundo, respectivamente. Las comparaciones y la muestra animada están en `verification/backdrop-polish/`.

La transparencia del agua se comprobó en 3-1, 3-2, 3-3, 3-8 y el acorazado del mundo 8. En 3-3 se compararon los píxeles secos y sumergidos de Mario durante una caída asistida y se verificó la superficie en 16:9, 21:9 y 32:9. La imagen de la PPU y las trazas del juego coinciden con las de la compilación anterior. Las capturas están en `verification/water/`.

## Camara, ventana adaptativa y mapas

Se revisaron los scripts de camara y ventana del PCK de SMB1 Remastered aportado por el usuario. Su viewport se adapta al area visible. No se ejecuto ni modifico su lanzador PortMaster; no se copio codigo GPL a esta integracion.

La camara de SMB3 avanzaba una pagina de mas al reconstruir el terreno: el scroll dibujado por la PPU se estaba interpretando como un anillo de 512 pixeles, mientras que este juego reutiliza columnas cada 256. Se corrigio esa conversion y se toma una instantanea coherente de RAM, OAM, paletas y RAM de cartucho para el fotograma visible. Las piezas de sprites que atraviesan un borde se colocan junto a su objeto, evitando su aparicion al otro lado del panoramico. El paralaje usa esa misma camara, tambien cuando cambia el ancho visible.

Se corrigio ademas un recorte de la ventana: el menu podia restaurar un viewport de 256 columnas despues de ampliar la imagen. `src/window.cpp` restablece el viewport de la imagen al copiarla a SDL, manteniendo la proporcion y el centrado de los formatos fijos. La adaptacion se compila sobre la copia de la imagen del host; los checkouts de dependencias permanecen intactos.

- **Desplazamiento**: secuencia en 1-1 con carrera y saltos hasta el fotograma 940, cruzando limites de paginas de 256, 512 y 768 pixeles. Se compararon capturas del fallo anterior y de la reconstruccion corregida.
- **Comparacion anterior de seis formatos**: antes de ampliar la simulacion, nativo, 1280x720, 1024x768, 1280x800, 2560x1080 y 3840x1080 conservaban la region comun y las trazas. Con la activacion panoramica, anchos distintos pueden cargar enemigos distintos y producir trazas distintas. La comprobacion actual de cambios de tamano se hace con la partida pausada.
- **Diez cambios de ventana con la partida pausada**: cinco tamanos en mapa y cinco en nivel. El ancho de presentacion cambia y tanto el contador de fotogramas como los 2048 bytes de RAM permanecen intactos. Los pixeles de la region comun del mapa y del nivel coinciden exactamente entre los cinco tamanos. Resultados: `verification/resolution/resize-result.json` y `paused-picture-parity.json`.
- **Prueba C++ de SDL sin ventana**: quince combinaciones de superficie y ancho de imagen, simulando que el menu restaura el viewport antiguo. Se comprueban pixeles a ambos extremos de la imagen y el centrado de los margenes. Se ejecuta con CTest y se incluye en el workflow Linux sin ROM.
- **Revision visual en X11**: selector, mapa y nivel completos, con Toadette, panel de informacion distribuido y cartas al lado derecho. Se guardaron `level-window.png`, `map-window.png` y `level-fullscreen.png` de 1920x1080 en `verification/resolution/`. Esta comprobacion usa el ejecutable sin la traza temporal empleada para diagnosticar SDL.

El modo inicial es Fit, con escala fraccionaria y vecino mas cercano. Los mapas usan paleta y sombras renovadas, fondo de terreno ampliado y el personaje seleccionado; los nodos y caminos originales se mantienen. El inventario se centra como un panel continuo durante el giro de la barra y el premio del cofre.

En la Casa de Toad se compararon 94 fotogramas del panel a 256, 426, 560 y 854 columnas. Se capturó el diálogo completo en una casa normal y en la blanca. Se comprobó la entrada a una tubería de 1-1 con los cuatro personajes y los siete trajes desde posiciones preparadas para la prueba. Se compararon también pantallas de pradera, agua, nieve y meta antes y después de optimizar el dibujado. Las capturas, hashes y la medición sin límite de fotogramas están en `verification/rooms-polish/`.

## Activacion y combate panoramicos

Los enemigos se buscan en el area visible y se mantienen activos hasta salir de ella, con un pequeno margen. Las rutinas originales siguen inicializando y actualizando los objetos. Los emisores y proyectiles de coordenada horizontal de ocho bits usan una proyeccion temporal al ejecutar sus rutinas originales; se conserva la coordenada completa y se comprueba la distancia real antes de una colision. La informacion adicional viaja en la RAM del cartucho guardada en los estados. Los bits bajos que el cargador de cofres consulta permanecen intactos.

Los sprites escritos por cada actor se asocian con su posicion completa, incluidas las piezas de cuerpos grandes. El terreno horizontal procede de una sola matriz de metatiles en toda la imagen, evitando la junta con el borde nativo. No se modifican la ROM ni los checkouts de dependencias.

`verify_widescreen.py` comprueba:

- Cuatro anchos de 256, 426, 560 y 854 columnas. La escena de prueba contiene uno, dos, tres y cinco actores activos respectivamente; se contrastan sus IDs con los datos del nivel y sus banderas de visibilidad.
- Bolas de fuego de piranha que atraviesan la coordenada 256 con movimiento continuo.
- Igualdad entre nativo e interprete en esa ruta y continuacion exacta al guardar en el fotograma 625 y cargar con un ataque activo.
- Un ataque a 256 pixeles del jugador no lo golpea; al colocarlo cerca si recibe dano. Estas dos colocaciones son asistidas.
- Emisores Bullet Bill y cannonball colocados de forma asistida fuera del cuadro NES, que producen ataques activos.
- Fuego del jugador que atraviesa el antiguo borde con traje y posicion asistidos para QA.
- Conservacion de los bits originales de los cofres durante las pruebas.

Tambien se compararon dos tramos de 300 fotogramas del castillo del mundo 8 con la ampliacion activa: nativo e interprete coinciden, el jugador no muere y todas las llamadas a las rutinas originales terminan correctamente. El resultado esta en `verification/wide-actors/broad/result.json`; son tramos de pasillos, no una pelea contra Bowser.

F11 se retiene desde el evento de tecla hasta que el host evalua los atajos; soltar la tecla entre fotogramas no pierde la pulsacion. La prueba C++ incluye pulsar y soltar antes de una evaluacion. La prueba de ventana X11 envia cuatro pulsaciones a la propia ventana y verifica la alternancia entre ventana y 1920x1080. Dos pulsaciones de Alt+Enter tambien alternan la pantalla completa con el selector abierto. No sustituye una prueba con el teclado fisico del usuario.

Los resultados actuales estan en `verification/wide-actors/result.json`, `verification/graphics/result.json`, `verification/checks/result.json` y `verification/resolution/resize-result.json`, cada uno con el SHA-256 del ejecutable comprobado. Los limites simultaneos de objetos de la ROM se mantienen. Los niveles verticales, dialogos, minijuegos y creditos conservan su composicion nativa. No hay editor, mapas propios ni gestor de resource packs. La campana completa hasta los creditos sigue sin estar certificada por una partida reproducible.

## Mapas, cierre y mecanicas en los bordes

Se adapto la propuesta visual del usuario a los colores y pixeles del juego: mapa centrado con marco, sombra y fondo calido; informacion a la izquierda y cartas a la derecha, sin el retrato central anterior. Se reviso la ventana X11 y se repitieron los diez cambios de tamano con la partida pausada.

El paralaje ahora usa la posicion del borde visible del mundo, en vez de desplazar la capa segun el origen del cuadro NES. Dos capturas con ese borde quieto, pero con el origen nativo en 0 y 80, conservan exactamente los 426x64 pixeles superiores del fondo. Una sala corta simulada conserva al jugador dentro de sus limites fisicos, aunque haya espacio decorativo alrededor. Resultado privado: `verification/behavior/parallax-result.json`.

La secuencia COURSE CLEAR usa los gráficos de texto y carta que dibuja la ROM sobre el escenario panorámico. El cielo y el suelo se conservan al recoger la carta. El telón en zigzag usa el negro puro de los fondos oscuros del pack. En la repetición de 1-1, el marco de la carta se desplaza un máximo de 4 píxeles por fotograma durante el acercamiento y el golpe. También se revisó la subida de 59 píxeles de la cámara al volar en una prueba asistida. Las capturas del encuadre están en `verification/intro/`.

En 1-1, se compararon el cielo y el suelo píxel a píxel durante el resultado en 16:9, 21:9 y 32:9. La imagen y la traza del juego coincidieron al guardar y reanudar durante el resultado. Se probó la secuencia especial de tres cartas iguales con dos cartas añadidas para la prueba. Los resultados de estas comparaciones están en `verification/goal-shadows/result.json`.

La prioridad OAM se resuelve antes de mezclar los sprites con el fondo. Un sprite oculto detrás del fondo también oculta a los sprites con menor prioridad, como sucede en la NES. Esa máscara se aplica también a la proyección de las sombras. La prueba C++ comprueba que la sombra se vea en el fondo y respete los píxeles opacos del terreno. En 17 capturas de 1-1, los píxeles del suelo coincidieron al comparar la escena con las sombras activadas y desactivadas.

Las comprobaciones adicionales de `verify_widescreen.py` usan colocaciones asistidas y verifican:

- Un caparazon cruza 256 y mata al Goomba cercano; otro Goomba separado por una pagina permanece vivo.
- Un caparazon rompe un ladrillo en la columna 256. La version anterior rebotaba en el mismo punto dejando el ladrillo intacto.
- Un Goomba situado en el margen panoramico y parcialmente fuera de la imagen por arriba conserva su estado y su indice durante 50 fotogramas. La version anterior lo eliminaba al tercer fotograma.
- Un Goomba que cae fuera del nivel permanece inactivo mientras su origen sigue visible; al desplazar la camara fuera de esa zona y volver, reaparece. Se usa el bit 6 de su bandera de aparicion, que la ROM no utiliza, para conservar esa espera en los estados de partida. Al cambiar a ancho nativo se libera esa marca y se restaura la politica original.
- Se entra a 1-3 y 1-4 seleccionando el nodo del mapa mediante RAM. Despues, 200 fotogramas de movimiento y saltos coinciden entre nativo e interprete en panoramico.
- En 1-4, el jugador supera la coordenada 300 sin perder su velocidad por el borde NES ni morir por el desbordamiento de su coordenada de pantalla. Su altura inicial se mantiene de forma asistida para aislar la comprobacion del limite frente a las plataformas del nivel.

Estas pruebas no cubren todas las salas ni todas las variantes de desplazamiento especial del juego. Las modificaciones respetan las rutinas originales de deteccion de suelo y la capacidad simultanea de objetos de la ROM.

## Inicio, selector y pausa

Con la ROM local se recorrieron el aviso, la intro completa y el salto de intro, el menú principal, Settings, el selector de los cuatro personajes, el mapa y 1-1. Las pruebas de SDL usaron teclado en 1278 × 720 y un mando Xbox virtual en 1024 × 768. En la misma sesión se alternaron ambos dispositivos y se verificaron las indicaciones, la reasignación y el regreso desde la pausa al título.

La imagen permaneció idéntica entre dos capturas separadas por más de cuatro segundos en la pausa de 1-1. Se comprobó el mismo comportamiento en el mapa. La salida de audio mantuvo muestras de la música durante la pausa, también dentro de Settings, con una reducción de altas frecuencias. Al reanudar siguieron el movimiento y el temporizador. Estas comprobaciones corresponden al mapa y a 1-1.

## Resource packs — 25 de noviembre de 2026

El ejecutable lee `resource_packs/` junto a él, como Super Mario Bros. Remastered. Con las cuatro descargas originales (SMB3-mastered v0.9 extraído, hoja 143355, hoja WebP de Newgrounds y el ZIP de Vryell extraído) genera en memoria los atlas de campaña, selector, mapa y vuelo y los iconos de controles.

- **Paridad de imágenes**: los 14 PNG generados (3 de campaña, 4 de selector, 3 de mapa, 3 de vuelo y los iconos) coinciden píxel a píxel con el arte que preparaban los importadores Python, incluidos los valores RGB bajo transparencia.
- **Paridad en juego**: se instaló el runtime en una carpeta limpia que solo tenía `resource_packs/`. Se compararon 25 escenas con la compilación que usa `assets/remaster/`: nivel 1-1, mapa y los trajes Super, Mapache, Rana, Tanuki y Martillo para Luigi, Toad y Toadette, más el vuelo con Mapache de los cuatro personajes. Las 435 capturas Remastered 16:9 coinciden.
- **Sin packs**: Toad y Toadette no aparecen, `--character toad` termina con un mensaje que remite a README.md y el juego crea la carpeta vacía `resource_packs/`.

No se comprobó una partida completa con los packs, ni el ejecutable de Windows con rutas no ASCII dentro de `resource_packs/`.
