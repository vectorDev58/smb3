# Desarrollo

- Aplicar Ponytail: cambios pequenos y completos, nombres descriptivos, sin abstracciones para usos hipoteticos.
- Conservar validaciones, seguridad, manejo de errores y accesibilidad.
- Reutilizar NESRecomp y recomp-ui sin editar sus checkouts en `.deps/`.
- Usar la ROM del jugador para las poses, animaciones y máscaras originales. El arte de otros autores llega como resource packs en `resource_packs/`, junto al ejecutable, como en Super Mario Bros. Remastered. El ejecutable lo prepara al arrancar. Conserva sus créditos y su licencia.
- Los commits y paquetes de fuentes contienen código y configuración. Las ROM, los gráficos extraídos, los atlas adicionales, el C del juego generado y las partidas permanecen en rutas privadas e ignoradas por Git.
- Ejecutar `python3 -m unittest discover -s tests -v` tras modificar las herramientas.
- Con una ROM local, ejecutar `python3 tools/verify.py` tras cambiar configuracion, semillas o motor.
- Distinguir siempre cobertura automatizada, pantallas comprobadas y una partida terminada.
