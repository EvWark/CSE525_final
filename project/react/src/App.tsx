import { useState, useEffect, use } from 'react'
import Card from 'react-bootstrap/Card'
import Button from 'react-bootstrap/Button'
import Form from 'react-bootstrap/Form'
import axios from 'axios'
import './App.css'

interface Player {
  id: number
  name: string
  score: number
  attention: number
}

function App() {
  //const [playerName, setPlayerName] = useState('')
  const [searchTerm, setSearchTerm] = useState('')
  //const [gamemode, setGamemode] = useState('easy')
  //const [isActive, setIsActive] = useState(false)
  const [players, setPlayers] = useState<Player[]>([])
  const [result, setResult] = useState<Player | null>(null)

  const handleSearch = async (e : React.FormEvent<HTMLFormElement>) => {
    e.preventDefault()
    try {
      const response = await axios.get(`/api/players/?name=${searchTerm}`)
      const data = Array.isArray(response.data) ? response.data : response.data.results || []
      console.log('Fetched players:', data)
      setPlayers(data)
    } catch (error) {
      console.error('Error fetching players:', error)
    }
  }

  useEffect(() => {
  const socket = new WebSocket('ws://127.0.0.1:8000/ws/game/default/')
  socket.addEventListener('message', (event) => {
    const data = JSON.parse(event.data)
    console.log('Received WebSocket message:', data)
    setResult(data)
  })})
  

  /*const handleGameStart = (e : React.FormEvent<HTMLFormElement>) => {
    e.preventDefault()
    setIsActive(true)
    axios.post('/api/players/', { name: playerName, score: 0 , attention: 0 })
      .then(response => {
        console.log('Player created:', response.data)
      })
      .catch(error => {
        console.error('Error creating player:', error)
      })
  }*/

  return (
    <>
    {/* I don't think we need this atm, idk why I even wrote this in
    <Card>
      <Card.Body>
        <Card.Title>Game Settings</Card.Title>
        <Form onSubmit={handleGameStart}>
          <Form.Group>
            <Form.Label>Player Name</Form.Label>
            <Form.Control type="text" placeholder="Enter your name" value={playerName} onChange={(e) => setPlayerName(e.target.value)} />
          </Form.Group>
          <Form.Group>
            <Form.Label>Difficulty</Form.Label>
            <Form.Control as="select" value={gamemode} onChange={(e) => setGamemode(e.target.value)}>
              <option value="easy">Easy</option>
              <option value="medium">Medium</option>
              <option value="hard">Hard</option>
            </Form.Control>
          </Form.Group>
          <Form.Group>
            {isActive == true ? (
              <Button variant="danger" onClick={() => setIsActive(false)}>
                Stop Game
              </Button>
            ) : (
              <Button variant="primary" onClick={() => setIsActive(true)}>
                Start Game
              </Button>
            )}
          </Form.Group>
        </Form>
      </Card.Body>
    </Card>
    */}
    {result && (
      <Card>
        <Card.Body>
          <Card.Title>Results</Card.Title>
          <p>Name: {result.name}</p>
          <p>Score: {result.score}</p>
          <p>Attention: {result.attention}</p>
        </Card.Body>
      </Card>
    )}
    <Card> 
      <Card.Body>
        <Card.Title>Player Search</Card.Title>
        <Form onSubmit={handleSearch}>
          <Form.Group>
            <Form.Label>Search Players</Form.Label>
            <Form.Control type="text" placeholder="Enter player name" value={searchTerm} onChange={(e) => setSearchTerm(e.target.value)} />
          </Form.Group>
          <Button variant="primary" type="submit">
            Search
          </Button>
        </Form>
        <br></br>
        {players.length > 0 && (
          <div>
            <h5>Search Results:</h5>
            <ul>
              {players.map(player => (
                <li key={player.id}>{player.name} - Score: {player.score} - Attention: {player.attention}</li>
              ))}
            </ul>
          </div>
        )}
      </Card.Body>
    </Card>
    </>
  )
}

export default App
