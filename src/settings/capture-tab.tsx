import * as React from 'react';
import {Form} from "react-bootstrap";
import {CaptureMode} from "../native";
import {useEffect, useRef, useState} from "react";
import {ipcRenderer as ipc} from "electron";
import {FlatImageData} from "../shared";

const CaptureTab: React.FunctionComponent = () => {

    const [settings, setSettings] = useState<Alt1Settings>(defaultSettings()); // TODO: use previously saved settings
    const {captMode, overrideScale, captAutoDetect} = settings;

    const canvasRef = useRef<HTMLCanvasElement>(null);

    useEffect(() => {
        const interval = setInterval(() => {
            ipc.invoke('capture-full-rs')
                .then((result?: FlatImageData) => {
                    const canvas = canvasRef.current!;
                    if (result) {
                        const imageData = new ImageData(result.data, result.width, result.height);

                        canvas.width = imageData.width;
                        canvas.height = imageData.height;
                        const canvasCtx = canvas.getContext('2d');
                        canvasCtx?.putImageData(imageData, 0, 0);
                    } else {
                        const canvasCtx = canvas.getContext('2d');
                        canvasCtx?.clearRect(0, 0, canvas.width, canvas.height);
                    }
                })
                .catch(error => {
                    console.error(error);
                });
        }, 500);

        return () => clearInterval(interval);
    })

    const changeMode = (evt: React.ChangeEvent<HTMLInputElement>) => {
        setSettings(prevSettings => ({
            ...prevSettings,
            captMode: evt.target.value as CaptureMode
        }));
    };

    const changeOverrideScale = (evt: React.ChangeEvent<HTMLInputElement>) => {
        setSettings(prevSettings => ({
            ...prevSettings,
            overrideScale: evt.target.checked ? 1.0 : -1.0
        }));
    };

    const changeAutodetect = (evt: React.ChangeEvent<HTMLInputElement>) => {
        setSettings(prevSettings => ({
            ...prevSettings,
            captAutoDetect: evt.target.checked
        }));
    };

    return (
        <>
            <Form>
                <Form.Group className="mb-3">
                    <Form.Label>Capture mode</Form.Label>
                    <div style={{display: 'flex'}}>
                        <div style={{flexBasis: '150px', flexShrink: 0}}>
                            <Form.Check type="radio" label="OpenGL" value="opengl" onChange={changeMode}
                                        checked={captMode === 'opengl'}/>
                            <Form.Check type="radio" label="DirectX" value="window" onChange={changeMode}
                                        checked={captMode === 'window'}/>
                            <Form.Check type="radio" label="Desktop" value="desktop" onChange={changeMode}
                                        checked={captMode === 'desktop'}/>
                        </div>
                        <div style={{fontSize: '0.9rem'}}>
                            It is best to use DirectX or OpenGL capture mode, as these modes have good performance
                            and
                            can capture the RuneScape window even when other windows are hiding it.
                            These modes will also work with display scaling on Windows 8.1+ with high DPI screens.
                            Desktop capture can be used as a fallback method. It might not work in a lot of cases
                            and
                            will make some apps a lot slower.
                        </div>
                    </div>
                </Form.Group>
                <Form.Group className="mb-3">
                    <Form.Check type="checkbox" label="Override high DPI scaling detection"
                                checked={overrideScale === 1.0}
                                onChange={changeOverrideScale}/>
                    <Form.Check type="checkbox" label="Automatically detect capture mode"
                                checked={captAutoDetect}
                                onChange={changeAutodetect}/>
                </Form.Group>
            </Form>
            <div style={{display: 'flex', justifyContent: 'center'}}>
                <canvas ref={canvasRef} style={{flexBasis: '90%', maxWidth: '90%'}}/>
            </div>
        </>
    );
};

export default CaptureTab;