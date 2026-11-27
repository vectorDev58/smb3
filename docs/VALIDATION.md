# Validacion local — 23 de noviembre de 2026

Entorno comprobado: Linux x86_64, GCC 16.2, CMake 4.5, Python 3.14 y SDL 2.32.74. ROM USA PRG0 con SHA-256 `6ea0777ca520ba7ad7b0ea0f6452140d59aa0b1dffe5045cb1e8020dde10c267`. Se usaron las revisiones de `dependencies.json` y la PPU LLE de NESRecomp.

## Comprobaciones reproducibles

```sh
python3 -m unittest discover -s tests -v
python3 tools/verify.py
python3 tools/verify_graphics.py
python3 tools/verify_widescreen.py
python3 tools/verify_coop_pad.py
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

## Poses de personajes y menús — 26 de noviembre de 2026

La hoja 143355 no tiene la misma distribución para los cuatro personajes. Luigi tiene un grupo "SMA4 Jump" antes de "Carry Item" en los trajes Pequeño, Super, Fuego y Martillo. Toadette tiene una celda "Turn" antes de cada patada y un grupo "Carried" en la fila de la Rana. La tabla de poses ahora aplica esos desplazamientos por personaje.

- **Detección**: se midieron las celdas de cada fila por sus bordes y se comparó, para cada cuadro de la ROM entre `$00` y `$50`, la secuencia de celdas con la de Mario. También se compararon las siluetas de Toadette con Toad y de Luigi con Mario. Así aparecieron la patada de Toadette en los siete trajes, las poses de cargar y patear de Luigi pequeño, Super, Fuego y Martillo, la Toadette pequeña cargando, Toadette Rana en el zapato de Kuribo y el derrape de la Rana, recortado 4 píxeles a la derecha en los tres personajes.
- **En juego**: con `Player_Kick` ($0557) y `Player_IsHolding` ($06A4) mantenidos desde los estados de cada traje, se revisaron capturas de la patada en los siete trajes y de las poses de cargar en Pequeño, Super, Fuego y Martillo para Luigi, Toad y Toadette.
- **HUD**: el panel de estado ya no copia la columna izquierda de 8 píxeles que la NES deja del color del cielo en los niveles.

Menús revisados con el servidor TCP del ejecutable (teclas, capturas de ventana) a 768 × 720 y 1920 × 1080, en inglés y español:

- Un solo cursor: el ratón mueve la selección solo cuando se desplaza; teclado y mando la recuperan al pulsar.
- La pausa y las pantallas abiertas desde ella oscurecen el juego y usan texto claro.
- El menú del título ocupa una fila bajo el logo y ya no tapa a los personajes de la demostración.
- Las ayudas del selector de personaje están al pie de la pantalla; Controles termina antes del suelo del escenario; los títulos de Configuración, Controles y Pruebas comparten estilo.

`verify.py`, `verify_widescreen.py`, `verify_graphics.py`, las pruebas Python y `ctest` pasan. No se jugó la campaña completa; los zapatos de Kuribo y los cambios con ratón se comprobaron en atlas y capturas, no en una partida.

## HUD, partida guardada y Configuración — 26 de noviembre de 2026

- **HUD**: el negro de la barra de estado usa el mismo negro que la banda, sin la franja gris. La insignia del jugador 1 muestra L (Luigi, verde) o T (Toad celeste, Toadette rosa) solo cuando la barra dibuja exactamente la M de la ROM; en el mapa está una fila más arriba. El jugador 2 conserva la L original. Revisado en niveles y en el mapa.
- **Partida guardada**: al llegar al mapa (45 cuadros estables) se escribe `saves/campaign.cycstate` y `campaign.ini` (mundo y jugadores), salvo con `--no-save`, durante Pruebas o antes de empezar. Probado en una instalación aparte: partida nueva hasta el mapa, cerrar, reabrir, CONTINUAR con "MUNDO 1" en el selector y vuelta al mismo mapa. "Menú principal" en la pausa deja la sesión suspendida y CONTINUAR la retoma; se comprobó con el temporizador del nivel.
- **Configuración**: se ocultan "Continuar" y "Salir" de Sistema (ya están en la pausa y el título) y "Menú principal" desde el título. Cada opción y cada sección muestran una descripción, en inglés o español.
- **Sprites del pack**: se probó sustituir enemigos e ítems por las hojas de SMB3-mastered comparando siluetas con la CHR de la ROM. Las coincidencias eran correctas (Goomba, Piraña, Hermano Martillo, ítems), pero el resultado en juego difería en pocos píxeles de sombreado y podía mezclar mitades de un mismo objeto. No se incluyó.

## Nombres, textos, efectos y opciones visuales — 26 de noviembre de 2026

- **Nombre del personaje**: los recuadros "WORLD 1" y "GAME OVER" del mapa muestran LUIGI, TOAD o TOADETTE en lugar de MARIO para el jugador 1. Solo se cambia la imagen, como la insignia del HUD: la tabla de nombres, la PPU y las trazas no cambian, y `verify_graphics.py` sigue comprobándolo. Revisado en capturas con los cuatro personajes (recuadro del mundo) y con Toadette (game over forzado con vidas a 0 y tiempo a 1).
- **Textos escritos en la pantalla**: la reconstrucción panorámica de las salas usaba solo los datos del nivel y descartaba lo que la ROM escribe directamente en la tabla de nombres. Ahora se conserva la imagen de la PPU en cada celda que ya no coincide con la sala. Revisado: el castillo del rey del mundo 1 ("The King has been transformed!"), el rey rescatado y la carta de la princesa (escena provocada con `Player_FallToKing`), en remasterizado y en clásico a 21:9. La fuente del diálogo ya no sustituye al banco gráfico del suelo de la sala. Los diálogos de las casas de Toad y el resultado del nivel mantienen su tratamiento anterior.
- **Efectos al otro lado de 256**: la estrella del impacto del caparazón, los puntos, las monedas que salen de los bloques, los restos de ladrillo, el humo y las salpicaduras guardan una X de 8 bits. En cada fotograma emulado, un efecto nuevo se coloca junto a su origen, por orden: el bloque que el juego acaba de cambiar (`Level_BlockChgX`), el enemigo derribado o pateado, un cañón y el jugador, cada uno con una tolerancia de pocos píxeles. Después conserva esa copia mientras se mueve. Solo si su origen desaparece en ese mismo fotograma se usa el actor más cercano. Revisado con los fixtures de caparazón contra un Goomba y contra un ladrillo en el borde NES, fotograma a fotograma. Es una regla de dibujo: no cambia la simulación.
- **Estilo visual**: "Clásico NES" ya no desactiva la pantalla panorámica ni la reconstrucción de las salas. Usa los colores y patrones originales y un cielo liso; sombras, partículas, HUD y estrella siguen sus propias opciones. En ancho nativo las trazas de clásico y remasterizado siguen coincidiendo.
- **GAME OVER**: el recuadro dibuja al jugador pequeño de frente con el patrón `$61` en las ranuras 33 y 34; ahora se sustituye por la pose de frente del atlas del mapa. Revisado con Luigi, Toad y Toadette. Sin la hoja WebP del pack se conserva el sprite de la ROM.
- **Menú del título**: las opciones vuelven a estar apiladas bajo el logo, por preferencia del jugador; con partida guardada son cuatro filas y empiezan algo más arriba. Revisado en español a 768 × 720.
- **Opciones nuevas, como en Super Mario Bros. Remastered**: partículas de fondo, paralaje, HUD (paneles o barra NES) y efecto de estrella (clásico o arcoíris). Se guardan en `config.ini`. Revisado con el servidor TCP en ventana oculta: combinaciones en 1-1 y la sección Remastered de Configuración en español.

`python3 -m unittest`, `ctest`, `verify.py`, `verify_widescreen.py` y `verify_graphics.py` pasan. No se jugó una partida completa. No se revisaron todos los mundos ni las escenas de texto de los mundos 2 a 7. La grabación TAS de todos los niveles se desincroniza ya en el mapa del mundo 1 con esta versión y no sirvió para llegar a ellas.

## Cooperativo simultáneo (rama `coop`) — 26 de noviembre de 2026

SMB3 tiene un solo jugador por nivel. El compañero guarda sus variables de jugador (77 bytes de RAM, sacadas del desensamblado de Southbird) y se intercambian con las del líder para ejecutar el propio `Player_Update` de la ROM con el mando 2. Una dirección de retorno añadida a la pila hace que `Player_Update` se ejecute dos veces por fotograma, dentro del tiempo del juego. Cada objeto trata con el jugador más cercano, así que pisotones, patadas, daño y poderes usan el código original. El jugador en RAM lidera: la cámara lo sigue y el compañero no sale de su pantalla NES. Al entrar en tubos o puertas, tocar la meta o morir el líder, el compañero pasa a liderar. `Player_Die` se intercepta mientras el otro jugador sigue vivo. El compañero se dibuja con sus propios sprites, bancos de CHR y paleta de traje, y su estado va en las partidas guardadas y el rebobinado (registro `smb3.coop`).

- **Automático** (`verify_widescreen.py`, a 21:9): el compañero pisa al Goomba de 1-1 y suma los puntos; el mando 2 no mueve al jugador 1; si Mario choca con el Goomba, Luigi toma el relevo, sin música de muerte ni salida al mapa; las trazas del ejecutable nativo y del intérprete coinciden.
- **Capturas**: carrera y salto del compañero, golpe a un bloque "?", subida a los bloques grandes, bolas de fuego que derriban al Goomba (traje asignado para la prueba), relevo en los dos sentidos con la cámara siguiendo al nuevo líder y opción COOPERATIVO en la selección, en español.
- **Sin probar**: niveles con desplazamiento automático, salas verticales, agua, tuberías y puertas tomadas por el compañero, el zapato de Kuribo, la estatua Tanooki, los barcos, los jefes, el traje de rana y una partida completa. No se probó con dos mandos físicos.
- **Límites conocidos**: los proyectiles enemigos (martillos, fuego de las plantas) solo alcanzan al jugador que lidera. El compañero se ve con los gráficos de la ROM, sin el arte de los resource packs. El HUD muestra al líder. Morir con el otro jugador vivo no resta vidas.

## Cooperativo como en Super Mario Bros. Wonder (rama `coop`) — 26 de noviembre de 2026, tarde

- **Personaje del jugador 2**: tras elegir COOPERATIVO, el jugador 2 elige personaje (cualquiera salvo el del jugador 1; Toad y Toadette con los resource packs). Se guarda como `Partner` en `config.ini`; en pruebas, `--partner`.
- **Arte**: el compañero usa el mismo código de dibujo que el líder (poses, trajes y colores de los resource packs). Una corona marca al líder, al que sigue la cámara.
- **Fantasma y burbuja**: un jugador derrotado queda 5 segundos como fantasma donde cayó; si el líder lo toca, vuelve ahí. Si no, o si cayó a un hoyo, vuelve en una burbuja que flota hacia el líder y revienta al llegar. Un compañero que la pantalla deja atrás también vuelve en burbuja; antes lo arrastraba el borde, que podía meterlo en una pared. Si caen los dos, se pierde la vida.
- **Proyectiles enemigos**: martillos, fuego de plantas, bumeranes y balas de cañón tratan con el jugador más cercano y alcanzan al compañero.
- **Pruebas**: `--level` empieza en un nivel del modo práctica, por ejemplo `--level 3-3` o `--level "1-Airship"`.

Verificación:

- **Automático** (`verify_widescreen.py`, a 21:9), además de lo anterior: el fantasma del jugador 1 revive al tocarlo, un compañero rezagado vuelve en burbuja hasta el líder y el fuego de la piraña de 1-1 alcanza al compañero. El estado se lee del registro `smb3.coop` de partidas guardadas.
- **Recorrido automático de 88 niveles en cooperativo**: niveles numerados de los 8 mundos, fortalezas, dirigibles, tanques, barcos, castillo de Bowser, trampa de arena, pirámide, torre espiral, plantas piraña y emboscada. Duró 2400 fotogramas por nivel, con los dos jugadores corriendo y saltando y el jugador 1 con estrella. Ningún nivel se colgó ni dejó rutinas sin terminar. En 1-1, 1-Airship, 2-Fortress 1, 3-3, 4-4, 5-Airship, 6-1, 7-1 y 8-Tanks 1, las trazas nativas y del intérprete coinciden durante 1800 fotogramas. El recorrido encontró cuatro fallos, ya corregidos:
  - En 3-3, la burbuja no alcanzaba a un líder que estaba bajo el borde de la pantalla.
  - En la sala vertical de 7-1, el compañero se salía por el borde.
  - Un fantasma o una burbuja pasaban al nivel siguiente.
  - Faltaba el zapato de Kuribo bajo el compañero.
- **Capturas revisadas**:
  - Fantasma revivido y burbuja de vuelta en 1-1.
  - Fuego de la piraña sobre el compañero.
  - Luigi como compañero con cada traje: fuego (lanza y derriba un Goomba), mapache (vuela), rana, Tanuki (estatua y vuelta) y martillo.
  - Dirigible del mundo 1 con desplazamiento automático, agua en 3-3 y sala vertical de 7-1.
  - Selección del jugador 2 en el menú, con el servidor TCP en ventana oculta, y `Partner` guardado.
- **Un jugador sin cambios**: 4 personajes en pequeño, fuego y mapache, con 110 capturas de 1-1 cada uno, son idénticos píxel a píxel a la versión anterior.
- `python3 -m unittest`, `verify.py`, `verify_widescreen.py` y `verify_graphics.py` pasan.
- **Sin probar**:
  - El zapato de Kuribo real: al forzarlo, sus tiles no se cargan, tanto en el líder como en el compañero.
  - Los jefes, porque el piloto automático no llega a ellos.
  - Tuberías y puertas tomadas por el compañero.
  - Una partida completa y dos mandos físicos.
- **Límites**: el piloto automático muere pronto en muchos niveles, así que el recorrido cubre sobre todo sus primeros tramos. El HUD muestra al líder. Morir con el otro jugador en juego no resta vidas. El fantasma y la burbuja muestran al personaje de pie, en su forma grande.

## Mandos de cada jugador (rama `coop`) — 26 de noviembre de 2026, noche

Fallo reportado al jugar: con el jugador 1 en "Teclado + mando" y un solo mando conectado, el mando movía al jugador 1 en el mapa y el jugador 2 no tenía control. NESRecomp da a un jugador 1 de teclado el primer mando que nadie tenga asignado.

`CMakeLists.txt` hace que `cyc_sdl.c` lea los mandos a través de `smb3_input_eval` (`src/ui_host.c`), sin tocar `.deps/`. La asignación está en `src/pad_choice.h`:

- **Elegido en Controles**: la fila MANDO de cada jugador ofrece AUTO o uno de los mandos conectados. En la sesión vale ese mando exacto; `config.ini` guarda su modelo (`PlayerNDevice`), y al reiniciar el jugador toma el primer mando libre de ese modelo. Si un jugador elige el mando del otro, el otro pasa a AUTO. El mando vibra al elegirlo. La línea de estado dice qué mando usa el jugador.
- **Cooperativo con el jugador 1 en teclado**: si queda un solo mando libre, es del jugador 2, que lo conserva. Un mando conectado después va al jugador 1, así que nadie cambia de personaje. Esta asignación no se guarda en `config.ini`.
- **AUTO, como NESRecomp**: un jugador 1 de teclado toma el primer mando libre, luego cada jugador de mando el siguiente; un jugador 2 de teclado, el último.
- **Teclado**: cualquier jugador puede usar "Teclado + mando". Un jugador sin teclas propias (el 2 no tiene por defecto) recibe las del otro al pasar al teclado.

Mientras NESRecomp no reabre sus mandos tras conectar o desconectar uno, rige su propia asignación. Un jugador cuyo mando cambia se evalúa solo, como jugador 1 con ese mando; los atajos siguen leyendo todos los mandos.

Verificación:

- **Prueba unitaria** (`ctest`, `render_math`): mando solo para el jugador 1 sin cooperativo y para el jugador 2 con él, un segundo mando que aparece primero en el orden de SDL, el mando elegido, dos mandos iguales tras reiniciar, jugador 2 en teclado y jugador sin entrada.
- **Ventana oculta** (`verify_coop_pad.py`, servidor TCP y un mando virtual; los mandos reales quedan fuera): en cooperativo con el jugador 1 en teclado, el mando no mueve al jugador 1 en el mapa y el teclado sí. En 1-1 el mando mueve al compañero y no al jugador 1. Luego, por teclado: pausa → Configuración → Controles, mando 1 para el jugador 1 y jugador 2 a teclado. Al volver, el mando mueve al jugador 1 y el compañero no recibe nada. `config.ini` guarda `Player1Device`, deja vacío `Player2Device` y da al jugador 2 las teclas del 1. Con el binario anterior al arreglo, la prueba falla en el mapa, como en el fallo reportado.
- **Pantalla revisada** en capturas, en inglés: fila CONTROLLER en AUTO ("Uses no controller") y con el mando 1 elegido ("Uses controller 1: NESRecomp virtual controller").
- `ctest`, `python3 -m unittest`, `verify.py` y `verify_widescreen.py` pasan.
- **Encontrado y sin corregir**: Escape actúa dos veces en los menús del juego (tecla del menú y atajo de NESRecomp). Desde Controles vuelve directo a la pausa, y en la pausa no reanuda. Retroceso y el mando funcionan.
- **Sin probar**: mandos físicos, dos mandos a la vez (la prueba solo crea uno virtual), la vibración, conectar un mando a media partida y Windows (la prueba copia solo el ejecutable).
